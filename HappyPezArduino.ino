
#include <LiquidCrystal_I2C.h> // Pantalla lcd
#include <OneWire.h>           //sensor temperatura
#include <DallasTemperature.h> //sensor temperatura
#include <TimeLib.h>

#include "WiFiEsp.h"

#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(13, 12); // RX, TX
#endif

LiquidCrystal_I2C lcd(0x27, 20, 4);  // asigna direccion de interfaz I2C para pantalla Led de 20x4
#define ONE_WIRE_BUS 2               // pin en arduino sensor de temperatura
#define TEMPERATURE_PRECISION 9      // Lower resolution
OneWire oneWire(ONE_WIRE_BUS);       // comunicacion con el sensor de temperatura
DallasTemperature sensors(&oneWire); // referencia oneWire con DallasTemperature
byte numberOfDevices;                // Number of temperature devices found
DeviceAddress tempDeviceAddress;     // We'll use this variable to store a found device address

WiFiEspClient client;

byte status = WL_IDLE_STATUS;  // Estado de la red
char server[] = "happypez.tk"; // Servidor WS

//Variables desde la base de datos
byte tempMin = 0;
byte tempMax = 0;
byte phMin = 0;
byte phMax = 0;
short luzDiaMin = 0;
short luzDiaMax = 0;
short luzNocheMin = 0;
short luzNocheMax = 0;
short filtroMin = 0;
short filtroMax = 0;
short aireMin = 0;
short aireMax = 0;
short nivelAgua = 0;
short scalefactor = 0;
short sventilador = 0;
short sluz = 0;
short sfiltro = 0;
short saire = 0;

byte setDia = 0;   // Simula luz dia tarde y noche
byte setNoche = 0; // Simula luz dia tarde y noche
byte started = 0;

//char ssid[] = "Cano";            // Nombre del SSID (Red de internet wifi)
//char pass[] = "abcdefgh";        // Password de la red
char ssid[] = "iPhone de Pablo"; // Nombre del SSID (Red de internet wifi)
char pass[] = "aaaaaaaaaa";      // Password de la red

String fecha = "";
/* Configuracion de horarios. */

float tempC = 0;
float Ph = 0;
unsigned long currentMillis = 70000;
unsigned long prevMillisLectura = 0; // Guarda la ultima hora de Lectura de datos
unsigned long prevMillisGetConfig = 0;
unsigned long prevMillisAmanecer = 0;

long prevMillisPutData = 0; // Guarda la ultima hora de actualizacion de la bomba
long prevMillisLuz = 0;     // Guarda la ultima hora de actualizacion de la bomba
long prevMillisFiltro = 0;  // Guarda la ultima hora de actualizacion de la bomba
long prevMillisAire = 0;    // Guarda la ultima hora de actualizacion de la bomba

#define intervalPutData 10000 // Envia datos via wifi cada x milisegundos (5000 = 5 segundos)
#define intervalLuz 60000     // Revisa luz dia o noche cada x milisegundos (60000 = 60 segundos)
#define intervalFiltro 60000  // Revisa encendido u apagado cada x milisegundos
#define intervalAire 60000    // Revisa encendido u apagado cada x milisegundos

#define intervalLectura 5000    // Realiza mediciones cada x milisegundos
#define intervalGetConfig 60000 // Segundos de espera para consultar configuraciones

/* Fin configuracion horarios  */

#define idacuario 1

//Pin relay
#define rCalefactor 6
#define rAire 7
#define rFiltro 8

byte sw = 0;

//Pin cinta led
#define bluePin 10
#define greenPin 11
#define redPin 9

//Pin ventiladores
#define ventilador 4

//Pin sensor flotador nivel de agua
#define sNivelLiq 5
byte nivel = 0;

void setup()
{

  Serial.begin(9600);  // Inicializa puerto serial para monitor
  Serial1.begin(9600); // Inicializa puerto serial para WiFi

  /* Inicializa los pines en arduino 
 *  MODO OUTPUT: Solo envio de pulsos desde arduino hacia el dispositivo
 *  MODO INPUT: Solo lectura de datos desde el dispositivo, sensor, etc.
 */
  pinMode(ventilador, OUTPUT); //
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  pinMode(rCalefactor, OUTPUT);
  pinMode(rAire, OUTPUT);
  pinMode(rFiltro, OUTPUT);

  /* Inicializa lso pines de relay apagados  HIGH=Apagado , LOW= Encendido */
  digitalWrite(rCalefactor, HIGH);
  digitalWrite(rAire, HIGH);
  digitalWrite(rFiltro, HIGH);

  lcd.backlight();
  lcd.init();
  lcd.setCursor(1, 1);
  lcd.print(F("Iniciando sistema"));
  lcd.setCursor(5, 2);
  lcd.print(F("Happy Pez"));
  
  pinMode(sNivelLiq, INPUT); //sensor de nivel de liquido
  Serial.println(F("********* INICIANDO SISTEMA ***********"));

  conectarWifi();
  Serial.println(F("Sincronizando hora: "));
  getDate();
  getConfigAll();
  getArtefact();
  /* Inicializa pantalla led */


  sensors.begin(); //Inicia sensor de temperatura
  numberOfDevices = sensors.getDeviceCount();

  // Buscando cantidad de sensores
  Serial.print(F("Sensores de Temperatura encontrados: "));
  Serial.println(numberOfDevices, DEC);
  Serial.println();

  //Inicializa las luces apagadas
  analogWrite(redPin, 0);
  analogWrite(greenPin, 0);
  analogWrite(bluePin, 0);
}

int availableMemory()
{
  int size = 2048; // Use 2048 with ATmega328
  byte *buf;

  while ((buf = (byte *)malloc(--size)) == NULL)
    ;

  free(buf);

  return size;
}
//*************************************************
//*                                               *
//*                  LOOP SECTION                 *
//*                                               *
//*************************************************

void loop()
{

  currentMillis = millis();

  if ((currentMillis - prevMillisGetConfig > intervalGetConfig) || (started == 0))
  {
    Serial.println();
    currentMillis = millis();
    Serial.println(F("Sincronizando hora: "));
    getDate();
    Serial.println(F("Obtener configuraciones"));
    getConfigAll();
    getArtefact();
    prevMillisGetConfig = millis();
  }

  /********************************
 * Control horario para luz
 *******************************/
  currentMillis = millis();
  String horaActual = fecha.substring(11, 16);
  horaActual.replace(":", "");

  if ((currentMillis - prevMillisLuz > intervalLuz) || (started == 0))
  {
    Serial.println("LUZ DIA: " + horaActual + " - " + luzDiaMin + " - " + luzDiaMax);

    if ((horaActual.toInt() > luzDiaMin) && (horaActual.toInt() < luzDiaMax) && setDia == 0)
    {
      Serial.println("Enciende Dia");
      amanecer();
      setDia = 1;
      setNoche = 0;
    }
    Serial.println("LUZ DIA: " + horaActual + " - " + luzNocheMin + " - " + luzNocheMax);
    if (((horaActual.toInt() > luzNocheMin) || (horaActual.toInt() < luzNocheMax)) && (setNoche == 0))
    {
      Serial.println("Enciende Noche");
      anochecer();
      setNoche = 1;
      setDia = 0;
    }
    prevMillisLuz = millis();
  }
  /********************************
 * Control horario para Filtro
 *******************************/
  currentMillis = millis();
  if ((currentMillis - prevMillisFiltro > intervalFiltro) || (started == 0))
  {
    Serial.println("Hora Actual: " + horaActual + " > " + filtroMin + " < " + filtroMax);
    if ((horaActual.toInt() > filtroMin) && (horaActual.toInt() < filtroMax))
    {
      Serial.println("Enciende filtro");
      enciendeArtefacto(rFiltro);
    }
    else
    {
      Serial.println("Apaga filtro");
      apagaArtefacto(rFiltro);
    }
    prevMillisFiltro = millis();
  }

  currentMillis = millis();
  if ((currentMillis - prevMillisAire > intervalAire) || (started == 0))
  {
    Serial.println("Hora Actual: " + horaActual + " > " + aireMin + " < " + aireMax);
    if ((horaActual.toInt() > aireMin) && (horaActual.toInt() < aireMax))
    {
      Serial.println("Enciende Aire");
      enciendeArtefacto(rAire);
    }
    else
    {
      Serial.println("Apaga Aire");
      apagaArtefacto(rAire);
    }
    prevMillisAire = millis();
  }

  currentMillis = millis();
  if ((currentMillis - prevMillisLectura > intervalLectura) || (started == 0))
  {
    Serial.print(F("phMin: "));
    Serial.print(phMin);
    Serial.print(F("\tphMax: "));
    Serial.print(phMax);
    Serial.print(F("\ttempMin: "));
    Serial.print(tempMin);
    Serial.print(F("\ttempMax: "));
    Serial.print(tempMax);
    Serial.println();
    lecturaPh();
    NivelLiquido();
    if (numberOfDevices > 0)
    {
      lecturaTemp();
    }
    else
    {
      tempC = 0;
    }

    if (tempC > tempMax)
    {
      Serial.print(F("\tVentilador ON"));
      enciendeArtefacto(ventilador);
    }
    else
    {
      Serial.print(F("\tVentilador OFF"));
      apagaArtefacto(ventilador);
    }

    if (tempC < tempMin)
    {
      Serial.println(F("\tCalefactor: ON"));
      enciendeArtefacto(rCalefactor);
    }
    else
    {
      Serial.println(F("\tCalefactor: OFF"));
      apagaArtefacto(rCalefactor);
    }
    prevMillisLectura = millis();
    ;
    Serial.println(F("====================================================================================="));

    pantallaResult();
  }
  if (currentMillis - prevMillisPutData > intervalPutData)
  {
    prevMillisPutData = millis();
    putData();
  }
  started = 1;
}

void conectarWifi()
{
  /* Nuevo codigo conexion WiFi*/
  WiFi.init(&Serial1);
  while (status != WL_CONNECTED)
  {
    Serial.print(F("Conectandose a SSID: "));
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }
  Serial.println(F("Conexion exitosa: "));
}

int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void getDate()
{
  if (status != WL_CONNECTED)
  {
    conectarWifi();
  }

  if (!client.connect(server, 80))
  {
    Serial.println(F("ERROR - getDate: Connection failed"));
    conectarWifi();
    return;
  }
  client.println(F("GETDATE /ListenArduino.php?idacuario=1 HTTP/1.0"));
  client.println(F("Host: 201.238.201.51"));
  client.println(F("Connection: close"));

  if (client.println() == 0)
  {
    Serial.println(F("ERROR - getDate: Failed to send request"));
    return;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders))
  {
    Serial.println(F("Invalid response"));
    return;
  }
  fecha = "";
  byte i = 0;
  while (client.available())
  {
    char c = client.read();
    Serial.print(c);
    if (i < 19)
    {
      fecha = fecha + c;
    }
    i++;
  }
}

void getConfigAll()
{
  if (status != WL_CONNECTED)
  {
    conectarWifi();
  }

  if (!client.connect(server, 80))
  {
    Serial.println(F("Connection failed"));
    conectarWifi();
    return;
  }
  Serial.print(fecha);
  Serial.print(F(" - Buscando configuracion de parametros  -  "));
  client.println(F("GETCONFIG /ListenArduino.php?idacuario=1 HTTP/1.0"));
  client.println(F("Host: 201.238.201.51"));
  client.println(F("Connection: close"));

  if (client.println() == 0)
  {
    Serial.println(F("Failed to send request"));
    return;
  }
  else
  {
    Serial.println(F("OK send Request"));
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders))
  {
    Serial.println(F("Invalid response"));
    return;
  }

  char *buffId = (char *)malloc(16);
  char *buffVal = NULL;
  byte i = 0;
  byte j = 0;
  while (client.available())
  {
    char c = client.read();
    if (c != '|')
    {
      buffId[i] = c;
      i++;
      buffId[i] = '\0';
    }
    else
    {
      for (j = 0; j <= i; j++)
      {
        if (buffId[j] == ';')
        {
          buffId[j] = '\0';
          buffVal = buffId + j + 1;
        }
      }

      if (strcmp(buffId, "\"1") == 0)
      {
        tempMin = atoi(buffVal);
      }
      if (strcmp(buffId, "2") == 0)
      {
        tempMax = atoi(buffVal);
      }
      if (strcmp(buffId, "3") == 0)
      {
        phMin = atoi(buffVal);
      }
      if (strcmp(buffId, "4") == 0)
      {
        phMax = atoi(buffVal);
      }
      if (strcmp(buffId, "5") == 0)
      {
        luzDiaMin = atoi(buffVal);
      }
      if (strcmp(buffId, "6") == 0)
      {
        luzDiaMax = atoi(buffVal);
      }
      if (strcmp(buffId, "7") == 0)
      {
        luzNocheMin = atoi(buffVal);
      }
      if (strcmp(buffId, "8") == 0)
      {
        luzNocheMax = atoi(buffVal);
      }
      if (strcmp(buffId, "9") == 0)
      {
        nivelAgua = buffVal[0];
      }
      if (strcmp(buffId, "10") == 0)
      {
        filtroMin = atoi(buffVal);
      }
      if (strcmp(buffId, "11") == 0)
      {
        filtroMax = atoi(buffVal);
      }
      if (strcmp(buffId, "12") == 0)
      {
        aireMin = atoi(buffVal);
      }
      if (strcmp(buffId, "13") == 0)
      {
        aireMax = atoi(buffVal);
      }
      j = 0;
      i = 0;
    }
  }
  free(buffId);

  /* char* salida = (char*) malloc(5);
  salida[0]='H';salida[1]='o';salida[2]='l';salida[3]='a';salida[4]='\0';
  Serial.print(salida);
  free(salida);
  */
  Serial.println(F("==== Configuraciones ==== \n"));
  Serial.print(F("tempMin es: "));
  Serial.println(String(tempMin));
  Serial.print(F("tempMax es: "));
  Serial.println(String(tempMax));
  Serial.print(F("phMin es: "));
  Serial.println(String(phMin));
  Serial.print(F("phMax es: "));
  Serial.println(String(phMax));
  Serial.print(F("luzDiaMin es: "));
  Serial.println(String(luzDiaMin));
  Serial.print(F("luzDiaMax es: "));
  Serial.println(String(luzDiaMax));
  Serial.print(F("luzNocheMin es: "));
  Serial.println(String(luzNocheMin));
  Serial.print(F("luzNocheMax es: "));
  Serial.println(String(luzNocheMax));
  Serial.print(F("filtroMin es: "));
  Serial.println(String(filtroMin));
  Serial.print(F("filtroMax es: "));
  Serial.println(String(filtroMax));
  Serial.print(F("aireMin es: "));
  Serial.println(String(aireMin));
  Serial.print(F("aireMax es: "));
  Serial.println(String(aireMax));
  Serial.print(F("NivelAgua es: "));
  Serial.println(String(nivelAgua));

  client.flush();
  client.stop();
}

void getArtefact()
{
  if (status != WL_CONNECTED)
  {
    conectarWifi();
  }

  if (!client.connect(server, 80))
  {
    Serial.println(F("Connection failed"));
    conectarWifi();
    return;
  }
  Serial.print(fecha);
  Serial.print(F(" - Buscando configuracion de artefactos  -  "));
  client.println(F("GETARTEFACT /ListenArduino.php?idacuario=1 HTTP/1.0"));
  client.println(F("Host: happypez.tk"));
  client.println(F("Connection: close"));

  if (client.println() == 0)
  {
    Serial.println(F("Failed to send request"));
    return;
  }
  else
  {
    Serial.println(F("OK send Request"));
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders))
  {
    Serial.println(F("Invalid response"));
    return;
  }

  char *buffId = (char *)malloc(16);
  char *buffVal = NULL;
  byte i = 0;
  byte j = 0;
  while (client.available())
  {
    char c = client.read();
    if (c != '|')
    {
      buffId[i] = c;
      i++;
      buffId[i] = '\0';
    }
    else
    {
      for (j = 0; j <= i; j++)
      {
        if (buffId[j] == ';')
        {
          buffId[j] = '\0';
          buffVal = buffId + j + 1;
        }
      }
      Serial.println(F("Configuraciones"));
                Serial.print(buffId);
                Serial.print(" - ");
                Serial.println(buffVal);
      if (strcmp(buffId, "\"1") == 0)
      {
        scalefactor = atoi(buffVal);
      }
      if (strcmp(buffId, "2") == 0)
      {
        sventilador = atoi(buffVal);
      }
      if (strcmp(buffId, "3") == 0)
      {
        sluz = atoi(buffVal);
      }
      if (strcmp(buffId, "4") == 0)
      {
        sfiltro = atoi(buffVal);
      }
      if (strcmp(buffId, "5") == 0)
      {
        saire = atoi(buffVal);
      }
      j = 0;
      i = 0;
    }
  }
  free(buffId);
  Serial.println(F("==== Artefactos ==== \n"));
  Serial.print(F("Calefactor es: "));
  Serial.println(String(scalefactor));
  Serial.print(F("Ventilador es: "));
  Serial.println(String(sventilador));
  Serial.print(F("Luz es: "));
  Serial.println(String(sluz));
  Serial.print(F("Filtro es: "));
  Serial.println(String(sfiltro));
  Serial.print(F("Aire es: "));
  Serial.println(String(saire));

  client.flush();
  client.stop();
}
/*
void putData(){
  Serial.println();
    if (status != WL_CONNECTED)
  {
      conectarWifi();
  }
  Serial.println(F("PUTDATA Starting connection to server..."));
  // if you get a connection, report back via serial
  
    if (!client.connect(server, 80)) {
        Serial.println(F("ERROR - putData: Connection failed"));
        conectarWifi();
        return;
    }
      Serial.print(F(" PUTDATA  "));

      String peticionHTTP = "PUTDATA /MCS/Acuario/ListenArduino.php?idacuario=1&tempC=" + String(tempC) + "&ph=" + String(Ph) + "&nivel=ok";
      peticionHTTP = peticionHTTP + " HTTP/1.1\r\n";
      peticionHTTP = peticionHTTP + "Host: 201.238.201.51\r\n\r\n";
    
      client.println(peticionHTTP);
      client.println("Connection: close");
      client.println();
      
      while (client.available()) {
      char c = client.read();
      Serial.write(c);
      }

  // if the server's disconnected, stop the client
  if (!client.connected()) {
    Serial.println();
    Serial.println("Disconnecting from server...");
    client.stop();
  
}
*/

void putData()
{
  if (status != WL_CONNECTED)
  {
    conectarWifi();
  }

  if (!client.connect(server, 80))
  {
    Serial.println(F("ERROR - putData: Connection failed"));
    conectarWifi();
    return;
  }
  Serial.print(F("PUTDATA result: "));
  String peticion = "PUTDATA /ListenArduino.php?idacuario=1&tempC=" + String(tempC) + "&ph=" + String(Ph) + "&nivel=ok HTTP/1.0";

  client.println(peticion);
  client.println(F("Host: 201.238.201.51"));
  client.println(F("Connection: close"));

  if (client.println() == 0)
  {
    Serial.println(F("ERROR - Putdata: Failed to send request"));
    return;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders))
  {
    Serial.println(F("Invalid response"));
    return;
  }
  char *resp = "";
  byte i = 0;
  while (client.available())
  {
    char c = client.read();
    Serial.print(c);
    if (i < 19)
    {
      resp = resp + c;
    }
    i++;
  }
}

void lecturaTemp()
{
  sensors.requestTemperatures(); // Send the command to get temperatures
  // Loop through each device, print out temperature data
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
      tempC = sensors.getTempC(tempDeviceAddress);
      Serial.print(F("\tTemp C: "));
      Serial.print(tempC);

      // printTemperature(tempDeviceAddress); // Use a simple function to print out the data
    }
  }
}

void lecturaPh()
{
#define ph_pin A0 // A0 -> Po en placa del sensor
  if (analogRead(ph_pin) < 900)
  {
#define m_4 637 // lectura del sensor en solucion a ph4
#define m_7 537 // lectura del sensor en agua ph
    float measure = 0;
    float prom = 0;
    for (int i = 0; i < 20; i++)
    {
      measure = analogRead(ph_pin);
      prom = prom + measure;
    }
    prom = prom / 20;
    //calibracion
    Ph = 7.0 + ((prom - m_7) * 3 / (m_7 - m_4));
    Serial.print(F("PH1: "));
    Serial.print(Ph, 2);
  }
  else
  {
    Ph = 0;
  }
}

void pantallaResult()
{
  //Ph = random(  , 10);;
  //tempC = random(28.0, 3.02);;
  //float y = random(700, 790) / 100.0;
  //float tempC = random(3000, 3200) / 100.0;
  String fecha2;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PH   :");
  lcd.setCursor(6, 0);
  if (Ph == 0)
  {
    lcd.print("Not Connected");
  }
  else
  {
    lcd.print(Ph, 2);
  }
  lcd.setCursor(0, 1);
  lcd.print("Temp :");
  lcd.setCursor(6, 1);
  if (tempC == 85)
  {
    lcd.print("Not Connected");
  }
  else
  {
    lcd.print(tempC, 2);
  }
  lcd.setCursor(0, 2);
  lcd.print("Nivel:");
  lcd.setCursor(6, 2);
  if (nivel == 0)
  {
    lcd.print(F("OK"));
  }
  else
  {
    lcd.print(F("NOK"));
  }
  lcd.setCursor(0, 3);
  fecha2 = fecha.substring(0, 16);
  lcd.print(fecha2);
}

void NivelLiquido()
{
  nivel = 1;
  if (digitalRead(sNivelLiq) == HIGH)
  {
    nivel = 1;
  }
  else
  {
    nivel = 0;
  }
}

void amanecer()
{
  int r = 0;
  int g = 0;
  int b = 255;
  Serial.println("Inicia Amanecer" + String(r) + " " + String(g) + " " + String(b));
  for (r = 0; r < 256; r++)
  {
    setColor(r, g, b, "amanecer"); // red
    delay(100);
    g++;
  }
  Serial.println("Termina Amanecer" + String(r) + " " + String(g) + " " + String(b));
}

void atardecer()
{
  int r = 255;
  int g = 255;
  int b = 255;
  Serial.println("Inicia atardecer" + String(r) + " " + String(g) + " " + String(b));

  for (g = 255; g > 0; g--)
  {
    setColor(r, g, b, "atardecer"); // red
    delay(100);
    if (b > 0)
    {
      b--;
    }
    if (r > 220)
    {
      r--;
    }
  }
  Serial.println("Termina atardecer" + String(r) + " " + String(g) + " " + String(b));
}

void anochecer()
{
  int r = 255;
  int g = 255;
  int b = 255;
  Serial.println("Inicia anochecer" + String(r) + " " + String(g) + " " + String(b));
  for (r = 255; r > 0; r--)
  {
    setColor(r, g, b, "anochecer"); // red
    delay(100);
    g--;
  }
  Serial.println("Termina anochecer" + String(r) + " " + String(g) + " " + String(b));
}

void setColor(int red, int green, int blue, String etapa)
{
#ifdef COMMON_ANODE
  red = 255 - red;
  green = 255 - green;
  blue = 255 - blue;
#endif
  //Serial.println(etapa + " " + String(red) + " " + String(green) + " " + String(blue));
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

void enciendeArtefacto(byte artefacto)
{
  if (artefacto == 4)
  {
    digitalWrite(artefacto, HIGH);
  }
  else
  {
    digitalWrite(artefacto, LOW);
  }
}

void apagaArtefacto(byte artefacto)
{
  Serial.print("Apagando pin: " + artefacto);
  if (artefacto == 4)
  {
    digitalWrite(artefacto, LOW);
  }
  else
  {

    digitalWrite(artefacto, HIGH);
  }
}
