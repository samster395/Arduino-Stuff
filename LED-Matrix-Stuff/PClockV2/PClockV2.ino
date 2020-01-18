#include "time.h"
#include <Ticker.h>
#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "RemoteDebug.h"
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <FS.h>   // Include the SPIFFS library
#define ADAFRUIT_GFX_EXTRA 10
#include <PxMatrix.h>
#include <Fonts/TomThumb.h>

// Settings
#define logging 1
char* otaPass = "keepticking"; // The password needed to update over the air
const char* www_username = "admin";
const char* www_password = "keepticking";
char* Hostname = "PClock"; // The network name
const char* ntpServer = "pool.ntp.org"; // Should probably leave this alone
// Update time from NTP server every 5 hours
#define NTP_UPDATE_INTERVAL_SEC 5*3600
// End of Settings

String SWVer = "2";
bool Rotation;
int Brightness;
long gmtOffset_sec = 0;
int daylightOffset_sec = 0;
String CityID;
String APIKey;
String Units;
String TempU;
bool AutoBright;
int LightTimeout;
int Light_Threshold;
String AutoBrightTodo;
int AutoBrightBrightness;
String AutoBrightMode;
Ticker ticker1;
int32_t tick;

// flag changed in the ticker function to start NTP Update
bool readyForNtpUpdate = false;

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file, Creating new file");
      StaticJsonBuffer<400> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  //json["Hostname"] = "PClock";
  json["Rotation"] = true;
  json["Brightness"] = 255;
  //json["ntpServer"] = "pool.ntp.org";
  json["gmtOffset_sec"] = "0";
  json["daylightOffset_sec"] = "0";
  json["CityID"] = "";
  json["APIKey"] = "";
  json["Units"] = "Metric";
  json["AutoBright"] = true;
  json["LightTimeout"] = 10000;
  json["Light_Threshold"] = 20;
  json["AutoBrightTodo"] = "DO";
  json["AutoBrightBrightness"] = 30;
  json["AutoBrightMode"] = "BC";

  File configFile2 = SPIFFS.open("/config.json", "w");
  if (!configFile2) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile2);
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  //Hostname = json["Hostname"];
  Rotation = json["Rotation"];
  Brightness = json["Brightness"];
  //ntpServer = json["ntpServer"];
  gmtOffset_sec = json["gmtOffset_sec"];
  daylightOffset_sec = json["daylightOffset_sec"];
  CityID = json["CityID"].as<String>();
  APIKey = json["APIKey"].as<String>();
  Units = json["Units"].as<String>();
  AutoBright = json["AutoBright"];
  LightTimeout = json["LightTimeout"];
  Light_Threshold = json["Light_Threshold"];
  AutoBrightTodo = json["AutoBrightTodo"].as<String>();
  AutoBrightBrightness = json["AutoBrightBrightness"];
  AutoBrightMode = json["AutoBrightMode"].as<String>();

  configFile.close();
  return true;
}

bool saveConfig() {
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  //json["Hostname"] = Hostname;
  json["Rotation"] = Rotation;
  json["Brightness"] = Brightness;
  //json["ntpServer"] = ntpServer;
  json["gmtOffset_sec"] = gmtOffset_sec;
  json["daylightOffset_sec"] = daylightOffset_sec;
  json["CityID"] = CityID;
  json["APIKey"] = APIKey;
  json["Units"] = Units;
  json["LightTimeout"] = LightTimeout;
  json["AutoBright"] = AutoBright;
  json["LightTimeout"] = LightTimeout;
  json["Light_Threshold"] = Light_Threshold;
  json["AutoBrightTodo"] = AutoBrightTodo;
  json["AutoBrightBrightness"] = AutoBrightBrightness;
  json["AutoBrightMode"] = AutoBrightMode;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  configFile.close();
  Serial.println("Config Saved");
  return true;
}

ESP8266WebServer server(80);   //Web server object. Will be listening in port 80

RemoteDebug Debug;

#ifdef ESP32
#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#endif

#ifdef ESP8266

#include <Ticker.h>
Ticker display_ticker;
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_D 12
#define P_E 0
#define P_OE 2

#endif
// Pins for LED MATRIX

PxMATRIX display(32, 16, P_LAT, P_OE, P_A, P_B, P_C, P_D);
//PxMATRIX display(64,32,P_LAT, P_OE,P_A,P_B,P_C,P_D);
//PxMATRIX display(64,64,P_LAT, P_OE,P_A,P_B,P_C,P_D,P_E);

// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);

uint16_t myCOLORS[7] = {myRED, myGREEN, myBLUE, myWHITE, myYELLOW, myCYAN, myMAGENTA};

uint16_t static CallIM[] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0010 (16) pixels
  0xFFFF, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000,   // 0x0020 (32) pixels
  0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0x0000,   // 0x0030 (48) pixels
  0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000,   // 0x0040 (64) pixels
  0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0050 (80) pixels
};

uint16_t static NotiIM[] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000,   // 0x0010 (16) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000,   // 0x0020 (32) pixels
  0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0x0000,   // 0x0030 (48) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF,   // 0x0040 (64) pixels
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000,   // 0x0050 (80) pixels
};

uint16_t static ClearIM[] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0xFFE0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFE0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0010 (16) pixels
  0xFFE0, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0,   // 0x0020 (32) pixels
  0xFFE0, 0xFFE0, 0x0000, 0xFFE0, 0xFFE0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0xFFE0, 0x0000, 0x0000, 0x0000,   // 0x0030 (48) pixels
  0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFE0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFE0, 0x0000, 0x0000,   // 0x0040 (64) pixels
  0x0000, 0x0000, 0x0000, 0xFFE0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0050 (80) pixels
};

uint16_t static RainIM[] = {
  0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0010 (16) pixels
  0xFFFF, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0020 (32) pixels
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF,   // 0x0030 (48) pixels
  0x0000, 0x0000, 0x07FF, 0x0000, 0x07FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07FF, 0x0000, 0x07FF, 0x0000, 0x0000,   // 0x0040 (64) pixels
  0x0000, 0x0000, 0x0000, 0x07FF, 0x0000, 0x07FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07FF, 0x0000, 0x07FF, 0x0000,   // 0x0050 (80) pixels
};

uint16_t static ThunderstormIM[] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0xFFA0,   // 0x0010 (16) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0xFFA0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0xFFA0,   // 0x0020 (32) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0xFFA0, 0xFFA0, 0xFFA0, 0xFFA0, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0030 (48) pixels
  0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0x0000, 0x0000, 0x0000,   // 0x0040 (64) pixels
  0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFA0, 0xFFA0, 0x0000, 0x0000, 0x0000,   // 0x0050 (80) pixels
};

uint16_t static SnowIM[] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000,   // 0x0010 (16) pixels
  0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF,   // 0x0020 (32) pixels
  0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0x0000,   // 0x0030 (48) pixels
  0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0040 (64) pixels
  0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x0000,   // 0x0050 (80) pixels
};

uint16_t static DrizzleIM[] = {
  0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0010 (16) pixels
  0xFFFF, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000,   // 0x0020 (32) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0000, 0x0000, 0x07FF, 0x0000, 0x07FF, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0030 (48) pixels
  0x0000, 0x0000, 0x07FF, 0x0000, 0x07FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07FF, 0x0000, 0x07FF, 0x0000, 0x0000, 0x0000,   // 0x0040 (64) pixels
  0x0000, 0x0000, 0x0000, 0x07FF, 0x0000, 0x07FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0050 (80) pixels
};

uint16_t static CloudsIM[] = {
  0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0010 (16) pixels
  0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0020 (32) pixels
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF,   // 0x0030 (48) pixels
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000,   // 0x0040 (64) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0050 (80) pixels
};

uint16_t static AtmosphereIM[] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0638, 0x0638, 0x0000, 0x0000, 0x0000, 0x0638,   // 0x0010 (16) pixels
  0x0638, 0x0000, 0x0638, 0x0000, 0x0000, 0x0638, 0x0638, 0x0638, 0x0000, 0x0000, 0x0638, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0020 (32) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0030 (48) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0638, 0x0638, 0x0000, 0x0000, 0x0000, 0x0638, 0x0638, 0x0000, 0x0638,   // 0x0040 (64) pixels
  0x0000, 0x0000, 0x0638, 0x0638, 0x0638, 0x0000, 0x0000, 0x0638, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // 0x0050 (80) pixels
};

String Status = "ON";
String Mode = "TDW";
String Mode2 = "TLH";
String timeNOW;
String temperature;
String mintemp;
String maxtemp;
String condition;
String humidity;
String windsp;
int sunset;
int sunrise;

unsigned long next_weather_update = 0;
unsigned long loopdelay = 0;
unsigned long rotationtm = 0;
unsigned long weathRTT = 0;

enum state {
  OFF, COUNTDOWN, ON
};

state current_state;
unsigned long countdownMs;

#ifdef ESP8266
// ISR for display refresh
void display_updater()
{
  //  display.displayTestPixel(70);
  display.display(50);
}
#endif

#ifdef ESP32
void IRAM_ATTR display_updater() {
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  //isplay.display(70);
  display.displayTestPattern(70);
  portEXIT_CRITICAL_ISR(&timerMux);
}
#endif

const char MAIN_page[] PROGMEM = R"=====(
  <!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
body {
    /*background: #2c3e50ff;*/
    background-color: black;
}
.redt {
  color: red;
}
.center {
  text-align: center;
}
</style>
<title>PClock Control</title>
<body onload="BrightSL()">
<div class="center">

<h1 class="redt">PClock Control</h1>

<div class="redt" >
Auto Brightness: <button type="button" onclick="loadDoc('autobri', 'true')">On</button> <button type="button" onclick="loadDoc('autobri', 'false')">Off</button>
<br><br>
Brightness: <a id="sliderv"></a><br>
<input id="bright" type="range" min="1" max="255" value="128">

<button type="button" onclick="loadDoc('bright', document.getElementById('bright').value)">Submit</button>
<br><br>
<button type="button" onclick="loadDoc('chmode', '1')">Change Mode</button>
<br><br>
Display: <button type="button" onclick="loadDoc('status', 'ON')">On</button> <button type="button" onclick="loadDoc('status', 'OFF')">Off</button>
<br><br>
Mode Rotation: <button type="button" onclick="loadDoc('rotation', 'true')">On</button> <button type="button" onclick="loadDoc('rotation', 'false')">Off</button> 
<br><br><br>
<button onclick="window.location.href = '/settings';">Settings</button>
<br><br><br>
<button type="button" onclick="loadDoc('updw', '1')">Update Weather</button>
<br><br><br>
<button type="button" onclick="loadDoc('rst', '1')">Restart Clock</button>

<p id="response"></p>
</div>
</div>
<script>
var slider = document.getElementById("bright");
slider.addEventListener("input", BrightSL);

function BrightSL() {
 document.getElementById("sliderv").innerHTML = document.getElementById("bright").value;
}

function loadDoc(type, value) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange=function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("response").innerHTML = "Response: " + this.responseText;
    }
  };
  xhttp.open("GET", "/api?" + type +  "=" + value, true);
  xhttp.send();
}
</script>

</body>
</html>
)=====";

const char Settings_page[] PROGMEM = R"=====(
  <!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
body {
    /*background: #2c3e50ff;*/
    background-color: black;
}
.redt {
color: red;
}
.center {
  text-align: center;
}
</style>
<title>PClock - Settings</title>
<body>

<div class="center">

<h1 class="redt">PClock - Settings</h1>

<div class="redt" >

<button onclick="window.location.href = '/';">Go back</button><br><br><br><br>

<form id="set">
GMT offset in seconds: <input type="text" name="gmtOffset_sec" value=""><br><br>
Daylight savings offset in seconds: <input type="text" name="daylightOffset_sec" value=""><br><br>
<h3>More info on OpenWeatherMap <a href="https://openweathermap.org/current" target="_blank">here</a></h3>
OpenWeatherMap CityID: <input type="text" name="CityID" value=""><br><br>
OpenWeatherMap APIKey: <input type="text" name="APIKey" value=""><br><br>
Temperature Unit: <input type="text" name="Units" value=""> (Metric/C or Imperial/F<br><br>
Light Timeout: <input type="text" name="LightTimeout" value=""> How long to wait for the light level to stay below the threshold in milliseconds<br><br>
Light Threshold: <input type="text" name="Light_Threshold" value=""> This will vary depending on the resistor value used, I used an 100k<br><br>
Auto Brightness To Do: <input type="text" name="AutoBrightTodo" value=""> CM (Change mode and lower brightness) or DO (Display off), If you choose display off ignore the below settings<br><br>
AutoBright Brightness: <input type="text" name="AutoBrightBrightness" value=""> The brightness % to change to when auto brightness has been triggered<br><br>
AutoBright Mode: <input type="text" name="AutoBrightMode" value=""> The mode to change to when auto brightness has been triggered<br><br>
----------------------------------<br><br>
Admin Password: <input type="password" name="admpass" value=""> Admin password required to save config<br><br>
  <input type="submit" value="Save config">
</form> 

<p id="response"></p>
</div>
</div>

<script>
window.addEventListener("load", function () {
var xmlhttp = new XMLHttpRequest();
var url = "/shset";

xmlhttp.onreadystatechange = function() {
  if (this.readyState == 4 && this.status == 200) {
    var myArr = JSON.parse(this.responseText);
    setFI(myArr);
  //console.log(myArr);
  }
};
xmlhttp.open("GET", url, true);
xmlhttp.send();

function setFI(arr) {
  document.getElementsByName("gmtOffset_sec")[0].value = arr.gmtOffset_sec;
  document.getElementsByName("daylightOffset_sec")[0].value = arr.daylightOffset_sec;
  document.getElementsByName("CityID")[0].value = arr.CityID;
  document.getElementsByName("Units")[0].value = arr.Units;
  document.getElementsByName("LightTimeout")[0].value = arr.LightTimeout;
  document.getElementsByName("Light_Threshold")[0].value = arr.Light_Threshold;
  document.getElementsByName("AutoBrightTodo")[0].value = arr.AutoBrightTodo;
  document.getElementsByName("AutoBrightBrightness")[0].value = arr.AutoBrightBrightness;
  document.getElementsByName("AutoBrightMode")[0].value = arr.AutoBrightMode;
}

  function sendData() {
    var XHR = new XMLHttpRequest();

    // Bind the FormData object and the form element
    var FD = new FormData(form);

    // Define what happens on successful data submission
    XHR.addEventListener("load", function(event) {
      document.getElementById("response").innerHTML = "Response: " + event.target.responseText;
    });

    // Define what happens in case of error
    XHR.addEventListener("error", function(event) {
      alert("Oops! Something went wrong.");
    });

    // Set up our request
    XHR.open("POST", "/configs");

    // The data sent is what the user provided in the form
    XHR.send(FD);
  }
 
  // Access the form element...
  var form = document.getElementById("set");

  // ...and take over its submit event.
  form.addEventListener("submit", function (event) {
    event.preventDefault();

    sendData();
  });
});

</script>

</body>
</html>
)=====";

const char mediatype_html[] PROGMEM = "text/html";

void handleRoot() {
if (!server.authenticate(www_username, www_password)) {
  return server.requestAuthentication();
}
 server.send_P(200, mediatype_html, MAIN_page); //Send web page
}

void handleSettings() {
if (!server.authenticate(www_username, www_password)) {
  return server.requestAuthentication();
}
 server.send_P(200, mediatype_html, Settings_page); //Send web page
}

void handleConfigSave() {
  String message;
  if (server.arg("admpass") == www_password) {
  if (server.arg("gmtOffset_sec") == "") {} else { gmtOffset_sec = server.arg("gmtOffset_sec").toInt(); }
  if (server.arg("daylightOffset_sec" )== "") {} else { daylightOffset_sec = server.arg("daylightOffset_sec").toInt(); }
  if (server.arg("CityID") == "") {} else { CityID = server.arg("CityID"); }
  if (server.arg("APIKey") == "") {} else { APIKey = server.arg("APIKey"); }
  if (server.arg("LightTimeout") == "") {} else { LightTimeout = server.arg("LightTimeout").toInt(); }
  if (server.arg("Units") == "") {} else { Units = server.arg("Units"); }
  if (server.arg("Light_Threshold") == "") {} else { Light_Threshold = server.arg("Light_Threshold").toInt(); }
  if (server.arg("AutoBrightTodo") == "") {} else { AutoBrightTodo = server.arg("AutoBrightTodo"); }
  if (server.arg("AutoBrightBrightness") == "") {} else { AutoBrightBrightness = server.arg("AutoBrightBrightness").toInt(); }
  if (server.arg("AutoBrightMode") == "") {} else { AutoBrightMode = server.arg("AutoBrightMode"); }
  
    if (!saveConfig()) {
    message += ("Failed to save config");
  } else {
    message += ("Config saved");
  }
  } else {
    message = "Wrong password";
  }
  
 server.send(200, "text/plain", message);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

bool update_weather()
{

  HTTPClient http;  //Object of class HTTPClient
  http.begin("http://api.openweathermap.org/data/2.5/weather?id=" + CityID + "&appid=" + APIKey + "&units=" + Units);
  int httpCode = http.GET();
  //Check the returning code
  if (httpCode > 0) {
    // Parsing
    const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& root = jsonBuffer.parseObject(http.getString());
    // Parameters
    JsonObject& main = root["main"];
    float main_temp = main["temp"];
    float max_temp = main["temp_max"];
    float min_temp = main["temp_min"];
    int main_humidity = main["humidity"];
    float wind_speed = root["wind"]["speed"];
    JsonArray& weather = root["weather"];
    JsonObject& weather0 = weather[0];
    int weather0_id = weather0["id"];
    String weather0_main = weather0["main"];
    temperature = String(int(main_temp));
    mintemp =   String(int(min_temp));
    maxtemp =   String(int(max_temp));
    condition = weather0_main;
    humidity = String(int(main_humidity));
    windsp = String(int(wind_speed));
    JsonObject& sys = root["sys"];
    sunrise = sys["sunrise"];
    sunset = sys["sunset"];
  #ifdef logging
    // Output to serial monitor
    Debug.println("----");
    Debug.println(timeNOW);
    Debug.print("Cond:");
    Debug.println(condition);
    Debug.print("temp:");
    Debug.println(temperature + TempU);
    Debug.print("Humidity:");
    Debug.println(humidity);
    Debug.print("winsp:");
    Debug.println(windsp);
  #endif
  }
  http.end();   //Close connection
}

/*
  %a Abbreviated weekday name
  %A Full weekday name
  %b Abbreviated month name
  %B Full month name
  %c Date and time representation for your locale
  %d Day of month as a decimal number (01-31)
  %H Hour in 24-hour format (00-23)
  %I Hour in 12-hour format (01-12)
  %j Day of year as decimal number (001-366)
  %m Month as decimal number (01-12)
  %M Minute as decimal number (00-59)
  %p Current locale's A.M./P.M. indicator for 12-hour clock
  %S Second as decimal number (00-59)
  %U Week of year as decimal number,  Sunday as first day of week (00-51)
  %w Weekday as decimal number (0-6; Sunday is 0)
  %W Week of year as decimal number, Monday as first day of week (00-51)
  %x Date representation for current locale
  %X Time representation for current locale
  %y Year without century, as decimal number (00-99)
  %Y Year with century, as decimal number
  %z %Z Time-zone name or abbreviation, (no characters if time zone is unknown)
  %% Percent sign
  You can include text literals (such as spaces and colons) to make a neater display or for padding between adjoining columns.
  You can suppress the display of leading zeroes  by using the "#" character  (%#d, %#H, %#I, %#j, %#m, %#M, %#S, %#U, %#w, %#W, %#y, %#Y)
*/

char buffer[80];
char buffer2[80];
char buffer3[80];
char buffer4[80];

String timestring;
String dateDstring;
String dateMstring;

void LocalTimeDate() // Gets the time and date and saves it to strings, used for logging too
{
  time_t rawtime;
  struct tm * timeinfo;
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (buffer, 80, "%d/%m/%y %H:%M:%S", timeinfo);
  timeNOW = buffer;
  strftime (buffer2, 80, " %H:%M ", timeinfo);
  strftime (buffer3, 80, " %d", timeinfo);
  strftime (buffer4, 80, " %m", timeinfo);
  timestring = buffer2;
  timestring.trim();
  dateDstring = buffer3;
  dateDstring.trim();
  dateMstring = buffer4;
  dateMstring.trim();
}

void DisplayWeather()
{
  int imageHeight = 9;
  int imageWidth = 9;
  int counter = 0;
  int x = 2;
  int y = 0;
  for (int yy = 0; yy < imageHeight; yy++)
  {
    for (int xx = 0; xx < imageWidth; xx++)
    { 
      if (condition == "Clear") {
      display.drawPixel(xx + x , yy + y, ClearIM[counter]);
      } else if(condition == "Rain"){
        display.drawPixel(xx + x , yy + y, RainIM[counter]);
      } else if(condition == "Clouds"){
        display.drawPixel(xx + x , yy + y, CloudsIM[counter]);
      } else if(condition == "Drizzle"){
        display.drawPixel(xx + x , yy + y, DrizzleIM[counter]);
      } else if(condition == "Atmosphere" or condition == "Fog" or condition == "Mist" or condition == "Haze"){
        display.drawPixel(xx + x , yy + y, AtmosphereIM[counter]);
      } else if(condition == "Snow"){
        display.drawPixel(xx + x , yy + y, SnowIM[counter]);
      } else if(condition == "Thunderstorm"){
        display.drawPixel(xx + x , yy + y, ThunderstormIM[counter]);
      }
      counter++;
    }
  }

  display.setFont(&TomThumb);
  display.setTextColor(myWHITE);
  display.setCursor(15, 6);
  display.print(timestring);

    if (millis() > weathRTT)
  {
      if (Mode2 == "CTH")
  {
    Mode2 = "TLH";
  } else if (Mode2 == "TLH")
  {
    Mode2 = "CTH";
  }
    weathRTT = millis() + 15000; // 15 Seconds
  }

  if (Mode2 == "CTH")
  {
    display.setTextColor(myYELLOW);
    display.setCursor(1, 15);
    display.print(temperature+ TempU);

    display.setTextColor(myGREEN);
    display.setCursor(15, 15);
    display.print(humidity + "%");
  } else if (Mode2 == "TLH"){
    display.setTextColor(myCYAN);
    display.setCursor(1, 15);
    display.print(mintemp + TempU);

    display.setTextColor(myRED);
    display.setCursor(17, 15);
    display.print(maxtemp + TempU);
  }

}

void DisplayTimeDateWeather()
{
  display.setFont(&TomThumb);
  display.setTextColor(myWHITE);
  display.setCursor(15, 6);
  display.print(timestring);

  display.setTextColor(myYELLOW);
  display.setCursor(15, 15);
  display.print(dateDstring);

  // Draw date seperation line
  display.drawPixel(23, 9, myYELLOW);
  display.drawPixel(23, 10, myYELLOW);
  display.drawPixel(23, 11, myYELLOW);
  display.drawPixel(23, 12, myYELLOW);
  display.drawPixel(23, 13, myYELLOW);
  display.drawPixel(23, 14, myYELLOW);
  display.drawPixel(23, 15, myYELLOW);

  display.setTextColor(myYELLOW);
  display.setCursor(25, 15);
  display.print(dateMstring);

  //display.setTextColor(myCYAN);
  //display.setCursor(1,6);
  //display.print(condition);

  int imageHeight = 9;
  int imageWidth = 9;
  int counter = 0;
  int x = 2;
  int y = 0;
  for (int yy = 0; yy < imageHeight; yy++)
  {
    for (int xx = 0; xx < imageWidth; xx++)
    { 
      if (condition == "Clear") {
      display.drawPixel(xx + x , yy + y, ClearIM[counter]);
      } else if(condition == "Rain"){
        display.drawPixel(xx + x , yy + y, RainIM[counter]);
      } else if(condition == "Clouds"){
        display.drawPixel(xx + x , yy + y, CloudsIM[counter]);
      } else if(condition == "Drizzle"){
        display.drawPixel(xx + x , yy + y, DrizzleIM[counter]);
      } else if(condition == "Atmosphere" or condition == "Fog" or condition == "Mist" or condition == "Haze"){
        display.drawPixel(xx + x , yy + y, AtmosphereIM[counter]);
      } else if(condition == "Snow"){
        display.drawPixel(xx + x , yy + y, SnowIM[counter]);
      } else if(condition == "Thunderstorm"){
        display.drawPixel(xx + x , yy + y, ThunderstormIM[counter]);
      }
      counter++;
    }
  }

  display.setTextColor(myCYAN);
  display.setCursor(1, 15);
  display.print(temperature + TempU);

}

void DisplayBigClock()
{
  display.setFont();
  display.setTextColor(myWHITE);
  display.setCursor(1, 4);
  display.print(timestring);
}

void ChangeMode()
{
  if (Mode == "W")
  {
    Mode = "BC";
  } else if (Mode == "BC")
  {
    Mode = "TDW";
  } else if (Mode == "TDW")
  {
    Mode = "W";
  }
}

void showSet() {
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["Rotation"] = Rotation;
  json["Brightness"] = Brightness;
  json["gmtOffset_sec"] = gmtOffset_sec;
  json["daylightOffset_sec"] = daylightOffset_sec;
  json["CityID"] = CityID;
  json["Units"] = Units;
  json["AutoBright"] = AutoBright;
  json["LightTimeout"] = LightTimeout;
  json["Light_Threshold"] = Light_Threshold;
  json["AutoBrightTodo"] = AutoBrightTodo;
  json["AutoBrightBrightness"] = AutoBrightBrightness;
  json["AutoBrightMode"] = AutoBrightMode;
  String message;
  json.printTo(message);
  server.send(200, "application/json", message);
}


void showSet2() {
   File file2 = SPIFFS.open("/config.json", "r");
 
  if (!file2) {
    Serial.println("Failed to open file for reading");
    return;
  }
 
  Serial.println("File Content:");
 
  while (file2.available()) {
 
    Serial.write(file2.read());
  }
  Serial.println("");
 
  file2.close();
  server.send(200, "text/plain", "Check the serial consolse");
}

void handleSpecificArg() {

String message = "";

if (server.arg("chmode")== ""){     //Parameter not found

}else{     //Parameter found
ChangeMode();

message = "Mode Changed";
}
if (server.arg("updw")== ""){     //Parameter not found

}else{     //Parameter found
update_weather();

message = "Weather Updated";
}
if (server.arg("rst")== ""){     //Parameter not found

}else{     //Parameter found
message = "Restarting";
server.send(200, "text/plain", message);          //Returns the HTTP response
delay(2000);
ESP.restart();
}
if (server.arg("mode")== ""){     //Parameter not found

//message = "Mode Argument not found";

}else{     //Parameter found
Mode = server.arg("mode");

message = "Mode changed to ";
message += server.arg("mode");     //Gets the value of the query parameter
}
if (server.arg("status")== ""){     //Parameter not found

//message = "Mode Argument not found";

}else{     //Parameter found
Status = server.arg("status");

message = "Display turned ";
message += server.arg("status");     //Gets the value of the query parameter
}
if (server.arg("bright")== ""){     //Parameter not found

//message = "Mode Argument not found";

}else{     //Parameter found
Brightness = server.arg("bright").toInt();
saveConfig();

message = "Brightness changed to ";
message += server.arg("bright");     //Gets the value of the query parameter
}
if (server.arg("vers")== ""){     //Parameter not found

//message = SWVer;     //Gets the value of the query parameter

}else{     //Parameter found
  message = SWVer;
}
if (server.arg("rotation")== ""){     //Parameter not found

//message = "Mode Argument not found";

}else{     //Parameter found
  if (server.arg("rotation") == "true") {
    Rotation = true;
  } else {
    Rotation = false;
  }
  saveConfig();

message = "Rotation state: ";
message += server.arg("rotation");     //Gets the value of the query parameter
}
if (server.arg("autobri")== ""){     //Parameter not found

//message = "Mode Argument not found";

}else{     //Parameter found
  if (server.arg("autobri") == "true")
    AutoBright = true;
  else if (server.arg("autobri") == "false")
    AutoBright = false;
    current_state = OFF;
    Status = "ON";
saveConfig();
message = "Auto Brightness state: ";
message += server.arg("autobri");     //Gets the value of the query parameter
}
if (server.arg("noti")== ""){     //Parameter not found

//message = "Mode Argument not found";

}else{     //Parameter found
  message = "Received";
  if (Status == "ON" && current_state != ON){
  if (server.arg("noti") == "other"){
    display.clearDisplay();
    display.setFont(&TomThumb);
    display.setTextColor(myWHITE);
    display.setCursor(15,9);
    display.print("noti");
    display.setCursor(2,15);
    display.print("received");
    
  int imageHeight = 9;
  int imageWidth = 9;
  int counter = 0;
  int x = 2;
  int y = 0;
  for (int yy = 0; yy < imageHeight; yy++)
  {
    for (int xx = 0; xx < imageWidth; xx++)
    {
    display.drawPixel(xx + x , yy + y, NotiIM[counter]);
    counter++;
    }
  }
    server.send(200, "text/plain", message);          //Returns the HTTP response
    delay(10000);
    }
  else if (server.arg("noti") == "call") {
    display.clearDisplay();
    display.setFont(&TomThumb);
    display.setTextColor(myWHITE);
    display.setCursor(15,9);
    display.print("call");
    display.setCursor(3,15);
    display.print("incoming");
    
  int imageHeight = 9;
  int imageWidth = 9;
  int counter = 0;
  int x = 2;
  int y = 0;
  for (int yy = 0; yy < imageHeight; yy++)
  {
    for (int xx = 0; xx < imageWidth; xx++)
    {
    display.drawPixel(xx + x , yy + y, CallIM[counter]);
    counter++;
    }
  }
    server.send(200, "text/plain", message);          //Returns the HTTP response
    delay(28000);
  }
  }
}


server.send(200, "text/plain", message);          //Returns the HTTP response
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Mounting FS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  //if (SPIFFSformat) {
    //SPIFFS.format();
  //}
    if (!loadConfig()) {
    Serial.println("Failed to load config");
  } else {
    Serial.println("Config loaded");
  }
  WiFi.hostname(Hostname);
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.autoConnect(Hostname);

  // Initialize the server (telnet or web socket) of RemoteDebug
  Debug.begin(Hostname);
  // OR
  //Debug.begin(HOST_NAME, startingDebugLevel);
  // Options
  Debug.setResetCmdEnabled(true); // Enable the reset command
  // Debug.showProfiler(true); // To show profiler - time between messages of Debug

  String SWVerString = "SW VER: ";
  SWVerString += SWVer;
  Serial.println(SWVerString);

  ArduinoOTA.setHostname((const char *)Hostname);
  ArduinoOTA.setPassword((const char *)otaPass);
    ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  updateNTP(); // Init the NTP time

  tick = NTP_UPDATE_INTERVAL_SEC; // Init the NTP update countdown ticker
  ticker1.attach(1, secTicker); // Run a 1 second interval Ticker

  while (!time(nullptr))
  {
    Serial.print(".");
    delay(1000);
  }
  delay(1000);

  Serial.println("Time...");

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/settings", handleSettings);
  server.on("/configs", handleConfigSave);
  server.on("/api", handleSpecificArg);   //Associate the handler function to the path
  server.on("/shset", showSet);   //Associate the handler function to the path
  server.on("/shset2", showSet2);   //Associate the handler function to the path

  server.begin();                                       //Start the server
  Serial.println("Server listening");   

  String IPString = WiFi.localIP().toString();
  IPString.remove(0, 8);
  String IPString2 = "IP: ";
  IPString2 += IPString;

  display.begin(4);
  display.setBrightness(Brightness);
  display.setMuxPattern(STRAIGHT);
  display.setScanPattern(ZAGGIZ);
  display.clearDisplay();
  display.setFont(&TomThumb);
  display.setTextColor(myWHITE);
  display.setCursor(2,8);
  display.print(IPString2);
  
  display_ticker.attach(0.002, display_updater);
  //attachInterrupt(digitalPinToInterrupt(0),button_pressed,FALLING);
  //dimm=1;
  //yield();
  delay(3000);
}

boolean isLightAboveThreshhold() {
  //Serial.println(analogRead(A0));
  //Serial.println(Light_Threshold);
  return analogRead(A0) > Light_Threshold;
}

void loop()
{
  server.handleClient();    //Handling of incoming requests
  ArduinoOTA.handle();
  Debug.handle();

  if(readyForNtpUpdate)
   {
    readyForNtpUpdate = false;
    updateNTP();
    Serial.print("\nUpdated time from NTP Server: ");
  }

  if (Units == "Metric") {
  TempU = "C";
} else if (Units == "Imperial") {
  TempU = "F";
}
else {
  TempU = "C";
}

  if (millis() > loopdelay)
  {

    if (APIKey.length() < 1){
      display.clearDisplay();
      display.setFont(&TomThumb);
      display.setTextColor(myRED);
      display.setCursor(2,6);
      display.print("SETUP");
      display.setCursor(2,15);
      display.print("NEEDED");
    } else {
    
  display.setBrightness(Brightness);
  
  LocalTimeDate();
  
  // Update weather data every 10 minutes
  if (millis() > next_weather_update)
  {
    update_weather();
    next_weather_update = millis() + 1800000; // Thirty Minutes
    //next_weather_update = millis() + 10000; // Ten Seconds
  }
  
if (AutoBright) {
 switch (current_state) {
    case OFF:
      if (isLightAboveThreshhold()) {
        // do nothing;
      }
      else {
        countdownMs = millis();
        current_state = COUNTDOWN;
      }
      break;

    case COUNTDOWN:
      if (isLightAboveThreshhold()) {
        // don't need to perform any action to stop counting down
        current_state = OFF;
      }
      else if (millis() - countdownMs > LightTimeout) {
        if (AutoBrightTodo == "CM"){
          Brightness = AutoBrightBrightness;
          Mode = AutoBrightMode;
          //Serial.print("dark");
          current_state = ON;
        } else if (AutoBrightTodo == "DO") {
          Status = "OFF";
          current_state = ON;
        }
      }
      else {
        // do nothing
      }
      break;

    case ON:
      if (isLightAboveThreshhold()) {
        if (AutoBrightTodo == "CM"){
          Brightness = 255;
          Mode = "TDW";
          //Serial.print("bright");
          current_state = OFF;
       } else if (AutoBrightTodo == "DO") {
          Status = "ON";
          current_state = OFF;
       }
      }
      else {
        // do nothing
      }      break;
  } 
  }

  if (Rotation == true)
  {
    if (millis() > rotationtm)
  {
      if (Mode == "TDW")
  {
    Mode = "W";
    rotationtm = millis() + 30000; // 30 Seconds
  } else if (Mode == "W")
  {
    Mode = "TDW";
    rotationtm = millis() + 120000; // 120 Seconds
  }
    
  }
  } else {
    
  }


if (Status == "ON"){
  if (Mode == "TDW")
  {
    display.clearDisplay();
    DisplayTimeDateWeather();
  } else if (Mode == "BC")
  {
    display.clearDisplay();
    DisplayBigClock();
  } else if (Mode == "W"){
    display.clearDisplay();
    DisplayWeather();
  } else {
    display.clearDisplay();
    display.setFont(&TomThumb);
    display.setTextColor(myWHITE);
    display.setCursor(1,8);
    display.print("No Mode");
  }
} else {
display.clearDisplay();
}

 loopdelay = millis() + 1000;

    }
  }
}

// NTP timer update ticker
void secTicker()
{
  tick--;
  if(tick<=0)
   {
    readyForNtpUpdate = true;
    tick= NTP_UPDATE_INTERVAL_SEC; // Re-arm
   }

  // printTime(0);  // Uncomment if you want to see time printed every second
}


void updateNTP() {
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  delay(500);
  while (!time(nullptr)) {
    Serial.print("#");
    delay(1000);
  }
}
