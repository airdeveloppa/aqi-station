/**
AirCheckStation

Compiled for ESP32 dev kit v2.0.1 on Arduino IDE 1.8.19

Company: Developpa(Thailand) Co., Ltd.
Authors: Roberto Weiser
Copyright 2021

Version v1.0
**/

//********** LIBRARIES AND HEADERS**********//
#include <Adafruit_NeoPixel.h>
#include "PMS.h"
#include <WiFi.h>
#include "ThingSpeak.h"

//******* GPIO DEFINITIONS ******//
#define PIN_IO33 33
#define PIN_IO25 25
#define PIN_IO26 26
#define PIN_CTOUCH0 27
#define PIN_CTOUCH1 14
#define PIN_DI_BUTTON 12
#define PIN_DO_LED_AQI 13
#define PIN_RX_PM 15
#define PIN_TX_PM 4

//********** GLOBAL DEFINITIONS **********//
#define LED_NUM 2                  //Number of pixels on the device
#define PMS_SAMPLING_TIMES 30      //Number of samples taken before converting to AQI
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  600        /* Time ESP32 will go to sleep (in seconds) */
#define SECRET_CH_ID_AQI_STATION 1342504
#define INITIAL_DELAY 20000

//********** INIT LIBRARIES **********//
WiFiClient  wificlient;
PMS pms(Serial1);
PMS::DATA data;
Adafruit_NeoPixel pixels(LED_NUM, PIN_DO_LED_AQI, NEO_RGB + NEO_KHZ800);

//********** GLOBAL VARIABLES **********//
uint32_t green = pixels.Color(255, 0, 0);
uint32_t yellow = pixels.Color(215, 255, 0);
uint32_t orange = pixels.Color(69,255,0);
uint32_t red = pixels.Color(0, 255, 0);
uint32_t purple = pixels.Color(0,139,139);
uint32_t dark_purple = pixels.Color(0,102,51);
uint32_t blue = pixels.Color(0,0,255);

uint16_t valAQI;
uint16_t valAQI_PM2_5;
uint16_t valAQI_PM10;
uint16_t valPM2_5;
uint16_t valPM10;
uint16_t valPM1_0;
unsigned long buffPM2_5;
unsigned long buffPM10;
unsigned long buffPM1_0;
unsigned long counterPM;

const char WIFI_SSID[] = "CMR616";
const char WIFI_PASSWORD[] = "chiangmai123";

unsigned long AQIStationChannelNumber = SECRET_CH_ID_AQI_STATION;
const char* readAPIKey = "";
const char* WriteAPIKey = "";
String statusAQI = "";

//********** INIT PERIPHERALS **********//
void configPMS(void) {
  pms.wakeUp();
  pms.passiveMode();
}

void configSerial(void) {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, PIN_RX_PM, PIN_TX_PM);
}


void configWiFi(void) {
  WiFi.mode(WIFI_STA);
  // Connect or reconnect to WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    for(int i=0;i<3;i++){
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      delay(3000);
      if (WiFi.status() == WL_CONNECTED){
        Serial.println("\nWiFi Connected!");
        i =3;
      }
    }
  }
}

void configGPIO(void) {
  pinMode(PIN_DI_BUTTON, INPUT_PULLUP);
  pinMode(PIN_DO_LED_AQI, OUTPUT);
  pinMode(PIN_CTOUCH0, INPUT);
  pinMode(PIN_CTOUCH1, INPUT);
}

void configPixels(void) {
  pixels.begin();
  pixels.setBrightness(128);
  pixels.clear();
  pixels.show();
}

void IRAM_ATTR pushInterrupt() {

}

void setup(void) {

  configGPIO();
  configSerial();
  configPixels();
  configPMS();
  ThingSpeak.begin(wificlient);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}

void loop() {

  configWiFi();
  pms.wakeUp();
  delay(30000);
  collectData(PMS_SAMPLING_TIMES);
  calcAQI();
  AQIStatus();
  //displayColor();
  outputData2Serial();
  write2thingspeak();
  Sleep();
}

void write2thingspeak(void)
{
  ThingSpeak.setStatus(statusAQI);
  ThingSpeak.setField(1, valAQI);
  ThingSpeak.setField(2, valPM2_5);
  ThingSpeak.setField(3, valPM10);
  ThingSpeak.setField(4, valPM1_0);
  int x = ThingSpeak.writeFields(AQIStationChannelNumber,WriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}

void outputData2Serial(void)
{
    Serial.println(valPM2_5);
    Serial.println(valPM10);
    Serial.println(valPM1_0);
    Serial.println(valAQI);
}

void Sleep(void)
{
  Serial.println("Going to sleep");
  pms.sleep();
  esp_deep_sleep_start();
  Serial.println("Waking up");
}

void setLEDcolor(uint32_t color)
{
  pixels.setPixelColor(0,color);
  pixels.setPixelColor(1,color);
  pixels.show();
}

void fullOff(void) {

  for(uint16_t i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, pixels.Color(0,0,0,0 ) );
  }
  pixels.show();
}

void displayColor(void)
{
  if(valAQI<=50)
  {
    setLEDcolor(green);
  }
  else if(valAQI<=100 & valAQI>50)
  {
    setLEDcolor(yellow);
  }
  else if(valAQI<=150 & valAQI>100)
  {
    setLEDcolor(orange);
  }
  else if(valAQI<=200 & valAQI>150)
  {
    setLEDcolor(red);
  }
  else if(valAQI>200)
  {
    setLEDcolor(purple);
  }
}

void collectData(uint16_t samples)
{
  for (int i = 0;i < samples;i++)
  {
    pms.requestRead();
    if (pms.readUntil(data))
    {
      buffPM2_5 = buffPM2_5 + data.PM_AE_UG_2_5;
      buffPM10 = buffPM10 + data.PM_AE_UG_10_0;
      buffPM1_0 = buffPM1_0 + data.PM_AE_UG_1_0;
      counterPM++;
    }
  }
}

void calcAQI(void)
{
  valPM2_5 = buffPM2_5 / counterPM;
  buffPM2_5 = 0;
  valPM10 = buffPM10 / counterPM;
  buffPM10 = 0;
  valPM1_0 = buffPM1_0 / counterPM;
  buffPM1_0 = 0;
  counterPM = 0;

  if (valPM2_5 <= 12)
  {
    valAQI_PM2_5 = map(valPM2_5, 0, 12, 0, 50);
  }
  else if (valPM2_5 <= 35 & valPM2_5 > 12)
  {
    valAQI_PM2_5 = map(valPM2_5, 13, 35, 51, 100);
  }
  else if (valPM2_5 <= 55 & valPM2_5 > 35)
  {
    valAQI_PM2_5 = map(valPM2_5, 36, 55, 101, 150);
  }
  else if (valPM2_5 <= 150 & valPM2_5 > 55)
  {
    valAQI_PM2_5 = map(valPM2_5, 56, 150, 151, 200);
  }
  else if (valPM2_5 <= 250 & valPM2_5 > 150)
  {
    valAQI_PM2_5 = map(valPM2_5, 151, 250, 201, 300);
  }
  else if (valPM2_5 > 250)
  {
    valAQI_PM2_5 = map(valPM2_5, 251, 500, 301, 500);
  }

  if (valPM10 <= 54)
  {
    valAQI_PM10 = map(valPM10, 0, 54, 0, 50);
  }
  else if (valPM10 <= 154 & valPM10 > 55)
  {
    valAQI_PM10 = map(valPM10, 55, 154, 51, 100);
  }
  else if (valPM10 <= 155 & valPM10 > 254)
  {
    valAQI_PM10 = map(valPM10, 155, 254, 101, 150);
  }
  else if (valPM10 <= 354 & valPM10 > 255)
  {
    valAQI_PM10 = map(valPM10, 255, 354, 151, 200);
  }
  else if (valPM10 <= 424 & valPM10 > 355)
  {
    valAQI_PM10 = map(valPM10, 355, 424, 201, 300);
  }
  else if (valPM10 > 424)
  {
    valAQI_PM10 = map(valPM10, 424, 605, 301, 500);
  }

  if (valAQI_PM10 > valAQI_PM2_5)
  {
    valAQI = valAQI_PM10;
  }
  else
  {
    valAQI = valAQI_PM2_5;
  }
}

void AQIStatus(void)
{
  if (valAQI <= 50)
  {
    statusAQI = String("Good");
    //statusAQIchar[] = "Good";
  }
  else if (valAQI <= 100 & valAQI > 50)
  {
    statusAQI = String("Moderate");
    //statusAQIchar[] = "Moderate";
  }
  else if (valAQI <= 150 & valAQI > 100)
  {
    statusAQI = String("Unhealthy for Sensitive Groups");
    //statusAQIchar[] = "Unhealthy for Sensitive Groups";
  }
  else if (valAQI <= 200 & valAQI > 150)
  {
    statusAQI = String("Unhealthy");
    //statusAQIchar[] = "Unhealthy";
  }
  else if (valAQI <= 299 & valAQI > 200)
  {
    statusAQI = String("Very Unhealthy");
    //statusAQIchar[] = "Very Unhealthy";
  }
  else if (valAQI > 299)
  {
    statusAQI = String("Hazardous");
    //statusAQIchar[] = "Hazardous";
  }
}
