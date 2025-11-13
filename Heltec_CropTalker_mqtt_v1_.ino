#include <Arduino.h>
#include "TCA9548A.h"
TCA9548A I2CMux;
#include <GyverBME280.h>
GyverBME280 bme;
#include <Wire.h>
#include "RTClib.h"
RTC_DS3231 rtc;
#include "FS.h"
#include "SD.h"
#include "SPI.h"
SPIClass spi1;
#include "AS726X.h"
AS726X ams;
byte GAIN = 16;
byte MEASUREMENT_MODE = 3;
#include "VL53L1X_ULD.h"
VL53L1X_ULD TOF;
#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <Ticker.h>
Ticker blinker;

char dateString[11];
char timeString[9];
char sd_data[512];
bool state = true;
RTC_DATA_ATTR int bootCount = 0;
unsigned long unixtime;
uint16_t distance;
String MB_data,BME_data,rtc_data,up_ams_data,down_ams_data;
String file_header = "";
ModbusMaster node1,node2,node3;
uint8_t result1,result2,result3;
float Tsoil_1,Tsoil_2,Tsoil_3,SoilWC_1,SoilWC_2,SoilWC_3,vls,nir,PAR,NDVI,MSAVI,L,Tair,RH,Pressure;
//
#define SD_CS    20
#define SD_MOSI   6
#define SD_SCK   34
#define SD_MISO  33
#define MAX485_RE_NEG  48
#define MAX485_DE      47
#define SSERIAL_RX_PIN 26   //RO (Reciver output)
#define SSERIAL_TX_PIN 17  //DI (Drive input)
SoftwareSerial RS485Serial(SSERIAL_RX_PIN, SSERIAL_TX_PIN);
//
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.print (file.name());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.print(file.size());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}
//
void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}
//
void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}
//
void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}
//
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}
//
void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}
//
void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}
//
void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
      }
}
//
void fileInit(fs::FS &fs, const char * path, const char * filename) {

  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
  boolean flag = false;
  File directory = fs.open(path);

  if (directory) {
    Serial.println("Содержимое папки :");
    while (true) {
      File entry = directory.openNextFile();
      if (!entry) {
        break;  // Больше файлов нет
      }
      String nameFile = entry.path();
      //Serial.println(entry.name());  // Выводим название файла
      entry.close();                 // Закрываем файл

      if (nameFile != (filename)) {
      } else {
        flag = true;  // флаг для единоразовой записи строки названий столбцов
      }
    }
    if (flag) {
      appendFile(fs,filename, sd_data);
    } else {  
      writeFile(fs, filename, file_header.c_str());
    }
    directory.close();  // Закрываем папку
  } else {
    Serial.println("Ошибка открытия папки .");
  }
}
//
void up_ams_read(){
  //UP sensor
  Serial.println("Open port Up_AMS!");
  I2CMux.openChannel(3);
  delay(1000);
  if(ams.begin(Wire,GAIN, MEASUREMENT_MODE)){
    Serial.println("Up_AMS begin!");
  }else {Serial.println("Error Up_AMS begin!");}
  delay(1000);
  ams.takeMeasurements();
  //float Violet,Blue,Green,Yellow,Orange,Red,R,S,T,U,V,W;
  if (ams.getVersion() == SENSORTYPE_AS7262)
  {
    //Visible readings
    ams.getCalibratedViolet();
    ams.getCalibratedBlue();
    ams.getCalibratedGreen();
    ams.getCalibratedYellow();
    ams.getCalibratedOrange();
    ams.getCalibratedRed();
    
  }
    else if (ams.getVersion() == SENSORTYPE_AS7263)
  {
    //Near IR readings
    ams.getCalibratedR();
    ams.getCalibratedS();
    ams.getCalibratedT();
    ams.getCalibratedU();
    ams.getCalibratedV();
    ams.getCalibratedW();
  }
  //
  up_ams_data = String(ams.getCalibratedViolet())+";" +String(ams.getCalibratedBlue())+";" +String(ams.getCalibratedGreen())+";" 
    +String(ams.getCalibratedYellow())+";" +String(ams.getCalibratedOrange())+";" +String(ams.getCalibratedRed())+";"
    +String(ams.getCalibratedR())+";" +String(ams.getCalibratedS())+";" +String(ams.getCalibratedT())+";"
    +String(ams.getCalibratedU())+";" +String(ams.getCalibratedV())+";" +String(ams.getCalibratedW()); 
  //vls = (ams.getCalibratedViolet()+ams.getCalibratedBlue()+ams.getCalibratedGreen()+ams.getCalibratedYellow()+ams.getCalibratedOrange()+ams.getCalibratedRed()+ams.getCalibratedR());
  PAR = (-0.3+0.0391*(ams.getCalibratedViolet()+ams.getCalibratedBlue()+ams.getCalibratedGreen()+ams.getCalibratedYellow()+ams.getCalibratedOrange()+ams.getCalibratedRed()+ams.getCalibratedR()));
  if(PAR <= 0){
    PAR = 0;
  }
  nir=(ams.getCalibratedR()+ams.getCalibratedS()+ams.getCalibratedT()+ams.getCalibratedU()+ams.getCalibratedV()+ams.getCalibratedW());
  NDVI = (nir-ams.getCalibratedRed())/(nir+ams.getCalibratedRed());
  L = 1-((2*nir+1-sqrt(pow((2*nir-1),2)-8*(nir*ams.getCalibratedRed())))/2);
  MSAVI = 0.9*((nir-ams.getCalibratedRed())/(nir+ams.getCalibratedRed()-L))*(1+L);
  if(MSAVI < -1){
    Serial.println("MSAVI превышает нижний порог, значит MSAVI: ");
    MSAVI = -0.8;
    Serial.println(MSAVI);
  } else if(MSAVI >1){
    Serial.println("MSAVI превышает верхний порог, значит MSAVI: ");
    MSAVI = 1;
    Serial.println(MSAVI);
  }
  if(NDVI< 0){
    Serial.println("NDVI превышает нижний порог, значит NDVI: ");
    NDVI = -1;
    Serial.println(NDVI);
  } else if(NDVI >1){
    Serial.println("NDVI превышает верхний порог, значит NDVI: ");
    NDVI = 1;
    Serial.println(NDVI);
  }
  Serial.println("PAR");
  Serial.println(PAR);
  Serial.println("NDVI");
  Serial.println(NDVI);
  Serial.println("MSAVI_2");
  Serial.println(MSAVI);
  Serial.println("up_ams_data");
  Serial.println(up_ams_data);
  snprintf(sd_data, sizeof(sd_data),
          "%s;%s;%lu;%s;%s\n",
          dateString, timeString,unixtime,String("4B"), up_ams_data);
  Serial.println("Close port Up_AMS!");
  I2CMux.closeChannel(3);
}
//
void down_ams_read(){
  //Down sensor
  Serial.println("Open port Down_AMS!");
  I2CMux.openChannel(7);
  delay(500);
  if(ams.begin(Wire,GAIN, MEASUREMENT_MODE)){
    Serial.println("Down_AMS begin!");
  }else {Serial.println("Error Down_AMS begin!");}
  delay(1000);
  ams.takeMeasurements();
  //float Violet,Blue,Green,Yellow,Orange,Red,R,S,T,U,V,W;
  if (ams.getVersion() == SENSORTYPE_AS7262)
  {
    //Visible readings
    ams.getCalibratedViolet();
    ams.getCalibratedBlue();
    ams.getCalibratedGreen();
    ams.getCalibratedYellow();
    ams.getCalibratedOrange();
    ams.getCalibratedRed();
    
  }
    else if (ams.getVersion() == SENSORTYPE_AS7263)
  {
    //Near IR readings
    ams.getCalibratedR();
    ams.getCalibratedS();
    ams.getCalibratedT();
    ams.getCalibratedU();
    ams.getCalibratedV();
    ams.getCalibratedW();
    
  }
  //
  down_ams_data = String(ams.getCalibratedViolet())+";" +String(ams.getCalibratedBlue())+";" +String(ams.getCalibratedGreen())+";" 
    +String(ams.getCalibratedYellow())+";" +String(ams.getCalibratedOrange())+";" +String(ams.getCalibratedRed())+";"
    +String(ams.getCalibratedR())+";" +String(ams.getCalibratedS())+";" +String(ams.getCalibratedT())+";"
    +String(ams.getCalibratedU())+";" +String(ams.getCalibratedV())+";" +String(ams.getCalibratedW());
  Serial.println("down_ams_data");
  Serial.println(down_ams_data);
  delay(1000);
  nir=(ams.getCalibratedR()+ams.getCalibratedS()+ams.getCalibratedT()+ams.getCalibratedU()+ams.getCalibratedV()+ams.getCalibratedW());
  NDVI = (nir-ams.getCalibratedRed())/(nir+ams.getCalibratedRed());
  if(NDVI <0){
    NDVI = 0;
  }
  L = 1-((2*nir+1-sqrt(pow((2*nir-1),2)-8*(nir*ams.getCalibratedRed())))/2);
  MSAVI = ((nir-ams.getCalibratedRed())/(nir+ams.getCalibratedRed()-L))*(1+L);
  Serial.println("NDVI");
  Serial.println(NDVI);
  Serial.println("MSAVI_2");
  Serial.println(MSAVI);
  Serial.println("Close port Down_AMS!");
  I2CMux.closeChannel(7);
}
//
void TOF_read(){
  Serial.println("Open port TOF!");
  I2CMux.openChannel(4);
  delay(500);
  VL53L1_Error status = TOF.Begin();
  if (status != VL53L1_ERROR_NONE) {
    // If the sensor could not be initialized print out the error code. -7 is timeout
    Serial.println("Could not initialize the sensor, error code: " + String(status));
    while (1) {}
  }
  Serial.println("Sensor initialized");
  uint8_t x = 15;
  uint8_t y = 15;
  TOF.SetROI(x, y);
  uint16_t centerSPAD = 191;
  TOF.SetROICenter(centerSPAD);
  uint16_t buffer, buffer2;
  TOF.GetROI(&buffer,&buffer2);
  TOF.GetROICenter((uint8_t*)&buffer);
  TOF.StartRanging();
  uint8_t dataReady = false;
  while(!dataReady) {
    TOF.CheckForDataReady(&dataReady);
    delay(5);
  }
  // Get the results
  TOF.GetDistanceInMm(&distance);
  Serial.println("distance in mm");
  Serial.println(distance);
  //Serial.println("Size distance");
  //Serial.println(sizeof(distance));
  TOF.ClearInterrupt();
  Serial.println("Close port TOF!");
  I2CMux.closeChannel(4);
}
//
void RTC_read(){
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  DateTime now = rtc.now();
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  String dateString = String(now.day())+"." +String(now.month())+"." +String(now.year());
  String timeString = String(now.hour())+":" +String(now.minute())+":" +String(now.second());
  rtc_data = String(dateString)+";" +String(timeString)+";" +String(now.unixtime())+ ";" +String(rtc.getTemperature());
  Serial.println("rtc_data");
  Serial.println(rtc_data);
}
//
void BME280_read(){
  Serial.println("Open port BME!");
  I2CMux.openChannel(5);
  delay(10);
  if(bme.begin()){
    Serial.println("BME begin");
  } else {Serial.println("Error BME");}
  delay(200);
  Tair =bme.readTemperature();
  RH =bme.readHumidity();
  Pressure = bme.readPressure()/1000.0F;
  BME_data = String(bme.readTemperature())+";" +String(bme.readHumidity())+";" +String(bme.readPressure()/1000.0F);
  Serial.println("BME_data");
  Serial.println(BME_data);
  //Serial.println("Size BME_data");
  //Serial.println(sizeof(BME_data));
  delay(100);
  Serial.println("Close port BME!");
  I2CMux.closeChannel(5);
}
//
void preTransmission(){
  digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}
//
void postTransmission(){
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}
//
void ModBus(){
  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
  preTransmission();
  postTransmission();
  node1.begin(1,RS485Serial);
  node1.preTransmission(preTransmission);
  delay(20);
  node1.postTransmission(postTransmission);
  result1 = node1.readHoldingRegisters(0x0000, 2);
  state = !state;
  if (result1 == node1.ku8MBSuccess){
    Tsoil_1 = (node1.getResponseBuffer(1) / 10.00F);
    SoilWC_1 = (node1.getResponseBuffer(0) / 10.00F);
  }
  else{
    Serial.println("NOT Response  Sensor_1");
  }
  delay(100);
  node2.begin(2,RS485Serial);
  node2.preTransmission(preTransmission);
  delay(20);
  node2.postTransmission(postTransmission);
  result2 = node2.readHoldingRegisters(0x0000, 2);
  state = !state;
  if (result2 == node2.ku8MBSuccess){
    Tsoil_2 = (node2.getResponseBuffer(1) / 10.00F);
    SoilWC_2 = (node2.getResponseBuffer(0) / 10.00F);
  }
  else{
    Serial.println("NOT Response  Sensor_2");
  }
  delay(100);
  node3.begin(3,RS485Serial);
  node3.preTransmission(preTransmission);
  delay(20);
  node3.postTransmission(postTransmission);
  result3 = node3.readHoldingRegisters(0x0000, 2);
  state = !state;
  if (result3 == node3.ku8MBSuccess){
    Tsoil_3 = (node3.getResponseBuffer(1) / 10.00F);
    SoilWC_3 = (node3.getResponseBuffer(0) / 10.00F);
  }
  else{
    Serial.println("NOT Response  Sensor_3");
  }
  delay(100);
  //
  MB_data = String(Tsoil_1) + ";" +String(SoilWC_1) + ";" 
              +String(Tsoil_2) + ";" +String(SoilWC_2) + ";" 
              +String(Tsoil_3) + ";" +String(SoilWC_3);
  Serial.println("ModBus_data");
  Serial.println(MB_data);
  //Serial.println("Size ModBus_data");
  //Serial.println(sizeof(MB_data));
  delay(100);
}
//
void prepare_global_time(){
  
  DateTime now = rtc.now();
  float Tcell_C=rtc.getTemperature();
  unixtime=now.unixtime();
  snprintf(dateString, sizeof(dateString), "%02d.%02d.%04d",
          now.day(), now.month(), now.year());
  
  snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d",
          now.hour(), now.minute(), now.second());
}
//
/*###########################################MQTT###########################################*/
// 192.168.4.1 IP AP
#include <WiFi.h>
#include <PubSubClient.h>
#define LedPin 35
#define MAX_UID 8
const char* ssid = "HUAWEI_E5586_2D98";
const char* password = "7twcugre";
const char* mqtt_server = "94.154.11.74";
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];

const char* Tair_topic = "CropTalkerData/Tair";
const char* RH_topic = "CropTalkerData/RH";
const char* Pressure_topic = "CropTalkerData/Pressure";
const char* PAR_topic = "CropTalkerData/PAR";
const char* NDVI_topic = "CropTalkerData/NDVI";
const char* MSAVI_topic = "CropTalkerData/MSAVI";
const char* Distance_topic = "CropTalkerData/Distance";
const char* Tsoil1_topic = "CropTalkerData/Tsoil1";
const char* Tsoil2_topic = "CropTalkerData/Tsoil2";
const char* Tsoil3_topic = "CropTalkerData/Tsoil";
const char* SoilWC1_topic = "CropTalkerData/SoilWC1";
const char* SoilWC2_topic = "CropTalkerData/SoilWC2";
const char* SoilWC3_topic = "CropTalkerData/SoilWC3";
//
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //esp_wifi_disconnect();
  //WiFi.begin(ssid, password);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
   
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    
    //WiFi.begin(ssid, password);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
//
const char * generateUID(){
  /* Change to allowable characters */
  const char possible[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  static char uid[MAX_UID + 1];
  for(int p = 0, i = 0; i < MAX_UID; i++){
    int r = random(0, strlen(possible));
    uid[p++] = possible[r];
  }
  uid[MAX_UID] = '\0';
  return uid;
}
//
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "CropTalkerData/LED") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(LedPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(LedPin, LOW);
    }
  }
}
//
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    const char* UID = generateUID();
    if (client.connect(UID)) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("CropTalkerData");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    if(WiFi.status() != WL_CONNECTED){
      setup_wifi();
    }
    
  }
}
//
void mqttdata() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  //Serial.println("Checking time");
  if (now - lastMsg > 5000) {
    Serial.println("Time is ok");
    lastMsg = now;
    // Convert the value to a char array
    char tempString[11];
    dtostrf(Tair, 4, 2, tempString);
    Serial.print("t_air: ");
    Serial.println(Tair);
    client.publish("CropTalkerData/Tair", tempString);
    dtostrf(RH, 4, 2, tempString);
    Serial.print("RH: ");
    Serial.println(Tair);
    client.publish("CropTalkerData/RH", tempString);
    dtostrf(Pressure, 5, 2, tempString);
    Serial.print("Pressure: ");
    Serial.println(Pressure);
    client.publish("CropTalkerData/Pressure", tempString);
    dtostrf(distance, 4, 0, tempString);
    Serial.print("distance: ");
    Serial.println(distance);
    client.publish("CropTalkerData/Distance", tempString);
    dtostrf(PAR, 7, 2, tempString);
    Serial.print("par: ");
    Serial.println(PAR);
    client.publish("CropTalkerData/PAR", tempString);
    dtostrf(NDVI, 2, 2, tempString);
    Serial.print("NDVI: ");
    Serial.println(NDVI);
    client.publish("CropTalkerData/NDVI", tempString);
    dtostrf(MSAVI, 2, 2, tempString);
    Serial.print("MSAVI: ");
    Serial.println(MSAVI);
    client.publish("CropTalkerData/MSAVI", tempString);
    dtostrf(Tsoil_1, 5, 2, tempString);
    Serial.print("t_soil1: ");
    Serial.println(Tsoil_1);
    client.publish("CropTalkerData/Tsoil1", tempString);
    dtostrf(Tsoil_2, 5, 2, tempString);
    Serial.print("t_soil2: ");
    Serial.println(Tsoil_2);
    client.publish("CropTalkerData/Tsoil2", tempString);
    dtostrf(Tsoil_3, 5, 2, tempString);
    Serial.print("t_soil3: ");
    Serial.println(Tsoil_3);
    client.publish("CropTalkerData/Tsoil3", tempString);
    dtostrf(SoilWC_1, 5, 2, tempString);
    Serial.print("soilWC1: ");
    Serial.println(SoilWC_1);
    client.publish("CropTalkerData/SoilWC1", tempString);
    dtostrf(SoilWC_2, 5, 2, tempString);
    Serial.print("soilWC2: ");
    Serial.println(SoilWC_2);
    client.publish("CropTalkerData/SoilWC2", tempString);
    dtostrf(SoilWC_3, 5, 2, tempString);
    Serial.print("soilWC3: ");
    Serial.println(SoilWC_3);
    client.publish("CropTalkerData/SoilWC3", tempString);
    delay(10);
  }

}
//
void setup(){
  Serial.begin(115200);
  Serial.println("Я проснулся!!");
  pinMode(36,OUTPUT);
  Serial.println("Power Sensors ON");
  digitalWrite(36,HIGH);
  delay(200);
  Wire.begin(4,5);
  I2CMux.begin(Wire);
  I2CMux.closeAll();
  RS485Serial.begin(4800);
  SPIClass(1);
  spi1.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if(!SD.begin(SD_CS, spi1)){
    Serial.println("Card Mount Failed");
  } //else{appendFile(SD, "/TEST/DATA_TEST.txt","Date;Time;Record_№;UnixTime;Voltage_mV;Tcel_C;TS1;StWC1;TS2;StWC2;TS3;StWC3;TS4;StWC4;TS5;StWC5");}
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
  }
	uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  RTC_read();
  prepare_global_time();
  up_ams_read();
  TOF_read();
  BME280_read();
  ModBus();
  delay(1000);
  //down_ams_read();
  listDir(SD, "/", 0);
  char filename[23];
  sprintf(filename, "/DataLog/%c%c_%c%c_%c%c%c%c.txt", dateString[0],dateString[1],dateString[3],dateString[4],dateString[6],dateString[7],dateString[8],dateString[9]);
  //prepare_data_for_sd();
  snprintf(sd_data, sizeof(sd_data),
          "%s;%s;%lu;%s;%s;%u;%s\n",
          dateString, timeString,unixtime,String("4A"), BME_data, distance,MB_data);
  //blinker.attach(0.5,blink);
  
  fileInit(SD, "/DataLog",filename);
  delay(500);
  setup_wifi();
  Serial.println("Wifi is here");
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);
  delay(500);
  mqttdata();
  delay(2000);
  Serial.println("Power Sensors OFF");
  digitalWrite(35,LOW);
  Serial.println("Switch to deep sleep mode");
  esp_sleep_enable_timer_wakeup(900 * 1000000);
  esp_deep_sleep_start();

}

void loop(){}

































