// This software, the ideas and concepts is Copyright (c) David Bird 2021 and beyond.
// All rights to this software are reserved.

// *** DO NOT USE THIS SOFTWARE IF YOU CAN NOT AGREE TO THESE LICENCE TERMS ***

// It is prohibited to redistribute or reproduce of any part or all of the software contents in any form other than the following:
// 1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
// 2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
// 3. You may not, except with my express written permission, distribute or commercially exploit the content.
// 4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.
// 5. You MUST include all of this copyright and permission notice ('as annotated') and this shall be included in all copies or substantial portions of the software and where the software use is visible to an end-user.
 
// THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.
// FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// #################################################################################################################
#include <Arduino.h>            // In-built
#include <esp_task_wdt.h>       // In-built
#include "freertos/FreeRTOS.h"  // In-built
#include "freertos/task.h"      // In-built
#include "epd_driver.h"         // https://github.com/Xinyuan-LilyGO/LilyGo-EPD47 // DO NOT USE THIS LIBRARY IF BOUND BY THESE LICENCE TERMS
#include "esp_adc_cal.h"        // In-built
#include <ArduinoJson.h>        // https://github.com/bblanchon/ArduinoJson       // DO NOT USE THIS LIBRARY IF BOUND BY THESE LICENCE TERMS
#include <HTTPClient.h>         // In-built
//#include <WiFi.h>               // In-built
#include <SPI.h>                // In-built
#include <time.h>               // In-built
#include "owm_credentials.h"
#include "forecast_record.h"
#include "lang.h"
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic - default ip 192.168.4.1
#define SCREEN_WIDTH  EPD_WIDTH
#define SCREEN_HEIGHT EPD_HEIGHT
String version = "4.1 / 4.7in";
enum alignment {LEFT, RIGHT, CENTER};
#define White         0xFF
#define LightGrey     0xBB
#define Grey          0x88
#define DarkGrey      0x44
#define Black         0x00
#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false
boolean LargeIcon   = true;
boolean SmallIcon   = false;
#define Large  20           // For icon drawing
#define Small  10           // For icon drawing
String  Time_str = "--:--:--";
String  Date_str = "-- --- ----";
int     wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, EventCnt = 0, vref = 1100;
#define max_readings 24 // Limited to 3-days here, but could go to 5-days = 40 as the data is issued  
Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];
float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};
long SleepDuration   = 60; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int  WakeupHour      = 8;  // Wakeup after 07:00 to save battery power
int  SleepHour       = 23; // Sleep  after 23:00 to save battery power
long StartTime       = 0;
long SleepTimer      = 0;
long Delta           = 30; // ESP32 rtc speed compensation, prevents display at xx:59:yy and then xx:00:yy (one minute later) to save power
#include "opensans8b.h"
#include "opensans10b.h"
#include "opensans12b.h"
#include "opensans18b.h"
#include "opensans24b.h"
#include "moon.h"
#include "sunrise.h"
#include "sunset.h"
#include "uvi.h"
WiFiManager wifiManager;
GFXfont  newfont;
uint8_t *B;
int uS_TO_S_FACTOR = 1000000;  /* Conversion factor for micro seconds to seconds */
int TIME_TO_SLEEP = 3600;     /* Time ESP32 will go to sleep (in seconds) - 60 mins */
//int TIME_TO_SLEEP = 60;     /* Time ESP32 will go to sleep (in seconds) */

void BeginSleep() {
  epd_poweroff_all();
  UpdateLocalTime();
  if(debug){TIME_TO_SLEEP = 60;}
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Entering " + String(TIME_TO_SLEEP) + " (secs) of sleep time");
  Serial.println("Starting deep-sleep period...");
  uint64_t u64SleepLength = TIME_TO_SLEEP * 1000000UL;
  esp_sleep_enable_timer_wakeup(u64SleepLength);
  esp_deep_sleep_start();
}
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 
  setenv("TZ", Timezone, 1);
  tzset(); 
  delay(100);
  return UpdateLocalTime();
}
uint8_t StartWiFi() {
  wifiManager.autoConnect();
  if (WiFi.status() == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); 
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else Serial.println("WiFi connection *** FAILED ***");
  return WiFi.status();
}
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi switched Off");
}
void InitialiseSystem() {
  StartTime = millis();
  Serial.begin(115200);
  while (!Serial);
  Serial.println(String(__FILE__) + "\nStarting...");
  epd_init();
  B = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!B) Serial.println("Memory alloc failed!");
  memset(B, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
}
void loop() {}
void setup() {
  delay(500);
  InitialiseSystem();
  if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
    bool WakeUp = false;
    if (WakeupHour > SleepHour)
      WakeUp = (CurrentHour >= WakeupHour || CurrentHour <= SleepHour);
    else
      WakeUp = (CurrentHour >= WakeupHour && CurrentHour <= SleepHour);
    WakeUp = true; // delete unless debugging
    if (WakeUp) {
      byte Attempts = 1;
      bool RxWeather  = false;
      bool RxForecast = false;
      WiFiClient client;
      while ((RxWeather == false || RxForecast == false) && Attempts <= 2) { 
        if (RxWeather  == false) RxWeather  = obtainWeatherData(client, "onecall");
        if (RxForecast == false) RxForecast = obtainWeatherData(client, "forecast");
        Attempts++;
      }
      Serial.println("Received all weather data...");
      if (RxWeather && RxForecast) {
        epd_poweron();
        epd_clear();
        DisplayNews();
        DisplayQuote();
        StopWiFi();
        DisplayWeather();
        epd_draw_grayscale_image(epd_full_screen(), B);
        epd_poweroff_all(); // Switch off all power to EPD
      }
    } else {
      Serial.println("Sleep Hours Active");
    }
  }
  BeginSleep();
}
void Convert_Readings_to_Imperial() { 
  WxConditions[0].Pressure = hPa_to_inHg(WxConditions[0].Pressure);
  WxForecast[0].Rainfall   = mm_to_inches(WxForecast[0].Rainfall);
  WxForecast[0].Snowfall   = mm_to_inches(WxForecast[0].Snowfall);
}
bool DecodeWeather(String json, String Type) {
  const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(21) + 900;
  DynamicJsonDocument doc(32768);                      // allocate the JsonDocument
  //Serial.println();
  //Serial.println(doc.capacity());
  DeserializationError error = deserializeJson(doc, json); // Deserialize the JSON document
  if (error) {                                             // Test if parsing succeeds.
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  Serial.println(" Decoding " + Type + " data");
  if (Type == "onecall") {
    //WxConditions[0].High        = 50; // Minimum forecast low
    //WxConditions[0].Low         = 50;  // Maximum Forecast High
    WxConditions[0].FTimezone   = doc["timezone_offset"]; // "0"
    JsonObject current = doc["current"];
    WxConditions[0].Sunrise     = current["sunrise"];                              if(debug){Serial.println("SRis: " + String(WxConditions[0].Sunrise));}
    WxConditions[0].Sunset      = current["sunset"];                               if(debug){Serial.println("SSet: " + String(WxConditions[0].Sunset));}
    WxConditions[0].Temperature = current["temp"];                                 if(debug){Serial.println("Temp: " + String(WxConditions[0].Temperature));}
    WxConditions[0].FeelsLike   = current["feels_like"];                            if(debug){Serial.println("FLik: " + String(WxConditions[0].FeelsLike));}
    WxConditions[0].Pressure    = current["pressure"];                             if(debug){Serial.println("Pres: " + String(WxConditions[0].Pressure));}
    WxConditions[0].Humidity    = current["humidity"];                             if(debug){Serial.println("Humi: " + String(WxConditions[0].Humidity));}
    WxConditions[0].DewPoint    = current["dew_point"];                             if(debug){Serial.println("DPoi: " + String(WxConditions[0].DewPoint));}
    WxConditions[0].UVI         = current["uvi"];                                  if(debug){Serial.println("UVin: " + String(WxConditions[0].UVI));}
    WxConditions[0].Cloudcover  = current["clouds"];                               if(debug){Serial.println("CCov: " + String(WxConditions[0].Cloudcover));}
    WxConditions[0].Visibility  = current["visibility"];                           if(debug){Serial.println("Visi: " + String(WxConditions[0].Visibility));}
    WxConditions[0].Windspeed   = current["wind_speed"];                            if(debug){Serial.println("WSpd: " + String(WxConditions[0].Windspeed));}
    WxConditions[0].Winddir     = current["wind_deg"];                              if(debug){Serial.println("WDir: " + String(WxConditions[0].Winddir));}
    JsonObject current_weather  = current["weather"][0];
    String Description = current_weather["description"];                           // "scattered clouds"
    String Icon        = current_weather["icon"];                                  // "01n"
    WxConditions[0].Forecast0   = Description;                                     if(debug){Serial.println("Fore: " + String(WxConditions[0].Forecast0));}
    WxConditions[0].Icon        = Icon;                                            if(debug){Serial.println("Icon: " + String(WxConditions[0].Icon));}
  }
  if (Type == "forecast") {
   Serial.print(F("\nReceiving Forecast period - ")); 
    JsonArray list                    = root["list"];
    for (byte r = 0; r < max_readings; r++) {
      WxForecast[r].Dt                = list[r]["dt"].as<int>();
      WxForecast[r].Temperature       = list[r]["main"]["temp"].as<float>();       if(debug){Serial.println("Temp: " + String(WxForecast[r].Temperature));}
      WxForecast[r].Low               = list[r]["main"]["temp_min"].as<float>();    if(debug){Serial.println("TLow: " + String(WxForecast[r].Low));}
      WxForecast[r].High              = list[r]["main"]["temp_max"].as<float>();    if(debug){Serial.println("THig: " + String(WxForecast[r].High));}
      WxForecast[r].Pressure          = list[r]["main"]["pressure"].as<float>();   if(debug){Serial.println("Pres: " + String(WxForecast[r].Pressure));}
      WxForecast[r].Humidity          = list[r]["main"]["humidity"].as<float>();   if(debug){Serial.println("Humi: " + String(WxForecast[r].Humidity));}
      WxForecast[r].Icon              = list[r]["weather"][0]["icon"].as<char*>(); if(debug){Serial.println("Icon: " + String(WxForecast[r].Icon));}
      WxForecast[r].Rainfall          = list[r]["rain"]["3h"].as<float>();         if(debug){Serial.println("Rain: " + String(WxForecast[r].Rainfall));}
      WxForecast[r].Snowfall          = list[r]["snow"]["3h"].as<float>();         if(debug){Serial.println("Snow: " + String(WxForecast[r].Snowfall));}
      //if (r < 8) { // Check next 3 x 8 Hours = 1 day
      //  if (WxForecast[r].High > WxConditions[0].High) WxConditions[0].High = WxForecast[r].High; // Get Highest temperature for next 24Hrs
      //  if (WxForecast[r].Low  < WxConditions[0].Low)  WxConditions[0].Low  = WxForecast[r].Low;  // Get Lowest  temperature for next 24Hrs
      //}
    }
    float pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure; 
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; 
    WxConditions[0].Trend = "=";
    if (pressure_trend > 0)  WxConditions[0].Trend = "-";
    if (pressure_trend < 0)  WxConditions[0].Trend = "+";
    if (pressure_trend == 0) WxConditions[0].Trend = "O";

    if (Units == "I") Convert_Readings_to_Imperial();
  }
  return true;
}
String ConvertUnixTime(int unix_time) {
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm);
  }
  return output;
}
bool obtainWeatherData(WiFiClient & client, const String & RequestType) {
  const String units = (Units == "M" ? "metric" : "imperial");
  client.stop(); 
  HTTPClient http;
  String uri = "http://api.openweathermap.org/data/2.5/" + RequestType + "?lat=" + Latitude + "&lon=" + Longitude + "&appid=" + apikey + "&mode=json&units=" + units + "&lang=" + Language;
  if (RequestType == "Onecall") uri += "&exclude=minutely,hourly,alerts,daily";
  http.begin(client, uri); //http.begin(uri,test_root_ca); //HTTPS example connection
  Serial.print(uri);
  int httpCode = http.GET();
  String payload = http.getString();
  //Serial.println(payload);
  if (httpCode == HTTP_CODE_OK) {
    if (!DecodeWeather(payload, RequestType)) return false;
    client.stop();
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}
float mm_to_inches(float value_mm) {
  return 0.393701 * value_mm;
}
float hPa_to_inHg(float value_hPa) {
  return 0.2953 * value_hPa;
}
int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  // 'j' for dates in Julian calendar:
  j = k1 + k2 + d + 59 + 1;
  if (j > 22299160) j = j - k3; 
  return j;
}
int SumOfPrecip(float DataArray[], int readings) {
  float sum = 1;
  for (int i = 1; i <= readings; i++) sum += DataArray[i];
  return sum;
}
String TitleCase(String text) {
  if (text.length() == 0) {
    String temp_text = text.substring(0, 1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); 
  }
  else return text;
}

void DisplayNews() { 
  newfont = OpenSans18B;
  int y = 200;
  drawString(10, y, "Headlines", LEFT);

  HTTPClient http;
  http.begin("http://newsapi.org/v2/top-headlines?country=gb&apiKey=" + NewsApiK); //http.begin(uri,test_root_ca); //HTTPS example connection
  int httpCode = http.GET();
  String payload = http.getString();
  //Serial.println(payload);
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(20000);                      // allocate the JsonDocument
    DeserializationError error = deserializeJson(doc, payload); // Deserialize the JSON document
    if (error) {                                             // Test if parsing succeeds.
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
    }
    // convert it to a JsonObject
    JsonObject root = doc.as<JsonObject>();
    int NumberOfTimes = 6;
    for (int i=0; i < NumberOfTimes; i++)
    {      
      String Title = doc["articles"][i]["title"];
      if(Title.length() > 20) {
      if(debug){Serial.println(Title);}
      Title.remove(Title.lastIndexOf("-"));
      if(Title.length() > 84) {
        Title.remove(84);
        Title += "...";
      }
      y = y + 40;
      newfont = OpenSans10B;
      drawString(10, y, Title, LEFT);
      }
    }
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    http.end();
  }
  http.end();
}

void DisplayQuote() {
  //newfont = OpenSans18B;
  //drawString(10, 460, "Quote", LEFT);

  HTTPClient http;
  http.begin("http://api.quotable.io/random?maxLength=75"); //http.begin(uri,test_root_ca); //HTTPS example connection
  //http.addHeader("accept", "application/json");
  int httpCode = http.GET();
  String payload = http.getString();
  //Serial.println(payload);
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(1024);                      // allocate the JsonDocument
    DeserializationError error = deserializeJson(doc, payload); // Deserialize the JSON document
    if (error) {                                             // Test if parsing succeeds.
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
    }
    // convert it to a JsonObject
    JsonObject root = doc.as<JsonObject>();
    String Quote = doc["content"];
    String QLength = doc["length"];
    if(debug){Serial.println(Quote);}
    newfont = OpenSans12B;
    drawString(10, 500, Quote, LEFT);
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    http.end();
  }
  http.end();
}

void DisplayWeather() { 
  DisplayStatusSection(600, 20, wifi_signal);
  DisplayGeneralInfoSection(); 
  //DisplayAstronomySection(5, 252); 
  DisplayMainWeatherSection(10, 110);
}
void DisplayGeneralInfoSection() {
  newfont = OpenSans10B;
  drawString(5, 2, City, LEFT);
  //drawString(5, 2, "hello", LEFT);
  newfont = OpenSans8B;
  drawString(800, 40, "Updated: " + Time_str, LEFT);
  newfont = OpenSans18B;
  drawString(350, 2, Date_str, LEFT);
}
void DisplayMainWeatherSection(int x, int y) {
  newfont = OpenSans8B;
  DisplayTempHumiPressSection(x, y - 60);
  DisplayForecastTextSection(x, y + 45);
  DisplayVisiCCoverUVISection(400, y + 42);
}
String WindDegToOrdinalDirection(float winddirection) {
  if (winddirection >= 348.75 || winddirection < 11.25)  return TXT_N;
  if (winddirection >=  11.25 || winddirection < 33.75)  return TXT_NNE;
  if (winddirection >=  33.75 || winddirection < 56.25)  return TXT_NE;
  if (winddirection >=  56.25 || winddirection < 78.75)  return TXT_ENE;
  if (winddirection >=  78.75 || winddirection < 101.25) return TXT_E;
  if (winddirection >= 101.25 || winddirection < 123.75) return TXT_ESE;
  if (winddirection >= 123.75 || winddirection < 146.25) return TXT_SE;
  if (winddirection >= 146.25 || winddirection < 168.75) return TXT_SSE;
  if (winddirection >= 168.75 || winddirection < 191.25) return TXT_S;
  if (winddirection >= 191.25 || winddirection < 213.75) return TXT_SSW;
  if (winddirection >= 213.75 || winddirection < 236.25) return TXT_SW;
  if (winddirection >= 236.25 || winddirection < 258.75) return TXT_WSW;
  if (winddirection >= 258.75 || winddirection < 281.25) return TXT_W;
  if (winddirection >= 281.25 || winddirection < 303.75) return TXT_WNW;
  if (winddirection >= 303.75 || winddirection < 326.25) return TXT_NW;
  if (winddirection >= 326.25 || winddirection < 348.75) return TXT_NNW;
  return "?";
}
void DisplayTempHumiPressSection(int x, int y) {
  Serial.print(WxConditions[0].Temperature);
  newfont = OpenSans18B;
  drawString(x, y, String(WxConditions[0].Temperature, 1) + "°   " + String(WxConditions[0].Humidity, 0) + "%", LEFT);
  newfont = OpenSans12B;
  //DrawPressureAndTrend(x + 195, y + 15, WxConditions[0].Pressure, WxConditions[0].Trend);
  int Yoffset = 42;
  if (WxConditions[0].Windspeed > 0) {
    drawString(x, y + Yoffset, "FL: " + String(WxConditions[0].FeelsLike, 1) + "°    Hi/Lo: " + String(WxForecast[0].High, 0) + "°/" + String(WxForecast[0].Low, 0) + "°", LEFT);
    Yoffset += 30;
  }
  //drawString(x + 140, y + Yoffset, String(WxForecast[0].High, 0) + "° | " + String(WxForecast[0].Low, 0) + "° Hi/Lo", LEFT); // Show forecast high and Low
  //Yoffset += 30;
  int windsp = WxConditions[0].Windspeed * 2.237;
  drawString(x, y + Yoffset, String(windsp) + " MPH   " + WindDegToOrdinalDirection(WxConditions[0].Winddir), LEFT);
  //drawString(400, y, "Sunrise: " + ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5), LEFT);
  drawString(400, y + 42, "Sunset: " + ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5), LEFT);
}

void DisplayForecastTextSection(int x, int y) {
#define lineWidth 34
  newfont = OpenSans12B;
  String Wx_Description = WxConditions[0].Forecast0;
  Wx_Description.replace(".", ""); // remove any '.'
  int spaceRemaining = 0, p = 0, charCount = 0, Width = lineWidth;
  while (p < Wx_Description.length()) {
    if (Wx_Description.substring(p, p + 1) == " ") spaceRemaining = p;
    if (charCount > Width + 1) {
      Wx_Description = Wx_Description.substring(0, spaceRemaining) + "~" + Wx_Description.substring(spaceRemaining + 1);
      charCount = 0;
    }
    p++;
    charCount++;
  }
  if (WxForecast[0].Rainfall > 0) Wx_Description += " (" + String(WxForecast[0].Rainfall, 1) + String((Units == "M" ? "mm" : "in")) + ")";
  String Line1 = Wx_Description.substring(0, Wx_Description.indexOf("~"));
  String Line2 = Wx_Description.substring(Wx_Description.indexOf("~") + 1);
  drawString(x, y, TitleCase(Line1), LEFT);
  if (Line1 != Line2) drawString(x + 30, y + 30, Line2, LEFT);
}
void DisplayVisiCCoverUVISection(int x, int y) {
  newfont = OpenSans12B;
  //Visibility(x + 5, y, String(WxConditions[0].Visibility) + "M");
  Display_UVIndexLevel(x, y, WxConditions[0].UVI);
}
void Display_UVIndexLevel(int x, int y, float UVI) {
  String Level = "";
  if (UVI <= 2){              Level = " (L)";}
  if (UVI >= 3 && UVI <= 5){  Level = " (M)";}
  if (UVI >= 6 && UVI <= 7){  Level = " (H)";}
  if (UVI >= 8 && UVI <= 10){ Level = " (VH)";}
  if (UVI >= 11){             Level = " (EX)";}
  Serial.println("UVI Level:" + Level);
  drawString(x + 20, y - 5, String(UVI, (UVI < 0 ? 1 : 0)) + Level, LEFT);
  DrawUVI(x - 10, y - 5);
}
float NormalizedMoonPhase(int d, int m, int y) {
  int j = JulianDate(d, m, y);
  double Phase = (j + 4.867) / 29.53059;
  return (Phase - (int) Phase);
}
void DisplayAstronomySection(int x, int y) {
  newfont = OpenSans10B;
  time_t now = time(NULL);
  struct tm * now_utc  = gmtime(&now);
  drawString(x + 5, y + 102, MoonPhase(now_utc->tm_mday, now_utc->tm_mon + 1, now_utc->tm_year + 1900, Hemisphere), LEFT);
  DrawMoonImage(x + 10, y + 23);
  DrawMoon(x + 28, y + 15, 75, now_utc->tm_mday, now_utc->tm_mon + 1, now_utc->tm_year + 1900, Hemisphere); 
  drawString(x - 115, y - 40, ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5), LEFT);
  drawString(x - 115, y - 80, ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5), LEFT);
  DrawSunriseImage(x - 180, y - 20);
  DrawSunsetImage(x - 180, y - 60);
}
void DrawMoon(int x, int y, int diameter, int dd, int mm, int yy, String hemisphere) {
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "north") Phase = 1 - Phase;
  epd_fill_circle(x + diameter + 1, y + diameter, diameter / 2 + 1, DarkGrey, B);
  const int number_of_lines = 900;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++) {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = Xpos;
      Xpos2 = Rpos + 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos + 2 * Phase * Rpos + Rpos;
    }
    double pW1x = (Xpos1 - number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines + Ypos)  / number_of_lines * diameter + y;
    double pW2x = (Xpos2 - number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines + Ypos)  / number_of_lines * diameter + y;
    double pW3x = (Xpos1 - number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos - number_of_lines)  / number_of_lines * diameter + y;
    double pW4x = (Xpos2 - number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos - number_of_lines)  / number_of_lines * diameter + y;
    epd_draw_line(pW1x, pW1y, pW2x, pW2y, White, B);
    epd_draw_line(pW3x, pW3y, pW4x, pW4y, White, B);
  }
  epd_draw_circle(x + diameter + 1, y + diameter, diameter / 2, Black, B);
}
String MoonPhase(int d, int m, int y, String hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y++;
    m += 12;
  }
  ++m;
  c   = 365.25 * y;
  e   = 30.6  * m;
  jd  = c + e + d + 694039.09; 
  jd /= 29.53059; 
  b   = jd;
  jd  = b;   
  b   = jd * 8 + 0.5;
  b   = b & 7;
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return TXT_MOON_NEW;
  if (b == 1) return TXT_MOON_WAXING_CRESCENT;
  if (b == 2) return TXT_MOON_FIRST_QUARTER; 
  if (b == 3) return TXT_MOON_WAXING_GIBBOUS;
  if (b == 4) return TXT_MOON_FULL;
  if (b == 5) return TXT_MOON_WANING_GIBBOUS; 
  if (b == 6) return TXT_MOON_THIRD_QUARTER;
  if (b == 7) return TXT_MOON_WANING_CRESCENT;
  return "";
}
void DrawSegment(int x, int y, int o1, int o2, int o3, int o4, int o11, int o12, int o13, int o14) {
  epd_draw_line(x + o1,  y - o2,  x + o3,  y - o4,  Black, B);
  epd_draw_line(x + o11, y - o12, x + o13, y - o14, Black, B);
}
void DisplayStatusSection(int x, int y, int rssi) {
  newfont = OpenSans18B;
  //DrawRSSI(x + 305, y + 15, rssi);
  if(PowerSource == "Battery"){ DrawBattery(x + 75, y);}
}
void DrawPressureAndTrend(int x, int y, float pressure, String slope) {
  drawString(x + 25, y + 10, String(pressure, (Units == "M" ? 0 : 1)) + (Units == "M" ? "hPa" : "in"), LEFT);
  if      (slope == "-") {
    DrawSegment(x, y, 0, 0, 8, -8, 8, -8, 16, 0);
    DrawSegment(x - 1, y, 0, 0, 8, -8, 8, -8, 16, 0);
  }
  else if (slope == "0") {
    DrawSegment(x, y, 8, -8, 16, 0, 8, 8, 16, 0);
    DrawSegment(x - 1, y, 8, -8, 16, 0, 8, 8, 16, 0);
  }
  else if (slope == "+") {
    DrawSegment(x, y, 0, 0, 8, 8, 8, 8, 16, 0);
    DrawSegment(x - 1, y, 0, 0, 8, 8, 8, 8, 16, 0);
  }
}
void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 30;
    if (_rssi <= -40)  WIFIsignal = 24;
    if (_rssi <= -60)  WIFIsignal = 18;
    if (_rssi <= -80)  WIFIsignal = 12;
    if (_rssi <= -100) WIFIsignal = 6; 
    epd_fill_rect(x + xpos * 8, y - WIFIsignal, 6, WIFIsignal, Black, B);
    xpos++;
  }
}
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    vref = adc_chars.vref;
  }
  ///float voltage = analogRead(36) / 4096 * 6.566 * (vref / 1000);
  uint16_t v = analogRead(36);
  float voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
  //if (voltage > 1 ) {
    Serial.println("\nVoltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.2) percentage = 100;
    if (voltage <= 3.8) percentage = 0;  
    //epd_draw_rect(x + 25, y - 14, 40, 15, Black, B);
    //epd_draw_rect(x + 65, y - 10, 4, 7, Black, B);
    //epd_draw_rect(x + 27, y - 12, 36 * percentage / 100.0, 11, Black, B);
    drawString(x + 85, y - 14, String(percentage) + "%  " + String(voltage, 1) + "v", LEFT);
  //}
}
boolean UpdateLocalTime() {
  struct tm timeinfo;
  char   time_output[50], day_output[50], update_time[30];
  while (!getLocalTime(&timeinfo, 5)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");
  //if (Units == "M") {
    //sprintf(day_output, "%s, %02u %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 19 * 100);
    strftime(day_output, sizeof(day_output), "%a %b %d %Y", &timeinfo);
    strftime(time_output, sizeof(time_output), "%H:%M", &timeinfo); 
    //sprintf(time_output, "%s", update_time);
  //}
  //else
  //{
  //  strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo);
  //  strftime(update_time, sizeof(update_time), "%r", &timeinfo);
  //  sprintf(time_output, "%s", update_time);
  //}
  Date_str = day_output;
  Time_str = time_output;
  //Serial.println("Time: " + Time_str);
  return true;
}

void Visibility(int x, int y, String Visibility) {
  float start_angle = 0.52, end_angle = 2.61, Offset = 10;
  int r = 14;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    epd_draw_pixel(x + r - cos(i), 1 + y - r / 2 + r * sin(i) - Offset, Black, B);
    epd_draw_pixel(x + r - cos(i), y - r / 2 + r * sin(i) - Offset, Black, B);
  }
  start_angle = 3.61; end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    epd_draw_pixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i) + Offset, Black, B);
    epd_draw_pixel(x + r * cos(i), y + r / 2 + r * sin(i) + Offset, Black, B);
  }
  drawString(x + 20, y, Visibility, LEFT);
  epd_fill_circle(x, y + Offset, r / 4, Black, B);
}

void addmoon(int x, int y, bool IconSize) {
  int xOffset = 65;
  int yOffset = 12;
  if (IconSize == LargeIcon) {
    xOffset = 130;
    yOffset = -40;
  }
  epd_fill_circle(x - 16 + xOffset, y - 37 + yOffset, uint16_t(Small * 1.6), White, B);
  epd_fill_circle(x - 28 + xOffset, y - 37 + yOffset, uint16_t(Small * 1.0), Black, B);
}

void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) newfont = OpenSans24B; else newfont = OpenSans12B;
  drawString(x - 3, y - 10, "?", CENTER);
}

void DrawMoonImage(int x, int y) {
  Rect_t area = {.x = x, .y = y, .width  = moon_width, .height =  moon_height };
  epd_draw_grayscale_image(area, (uint8_t *) moon_data);
}

void DrawSunriseImage(int x, int y) {
  Rect_t area = {.x = x, .y = y, .width  = sunrise_width, .height =  sunrise_height};
  epd_draw_grayscale_image(area, (uint8_t *) sunrise_data);
}

void DrawSunsetImage(int x, int y) {
  Rect_t area = {.x = x, .y = y, .width  = sunset_width, .height =  sunset_height};
  epd_draw_grayscale_image(area, (uint8_t *) sunset_data);
}

void DrawUVI(int x, int y) {Rect_t area = {.x = x, .y = y, .width  = uvi_width, .height = uvi_height};
  epd_draw_grayscale_image(area, (uint8_t *) uvi_data);
}

void drawString(int x, int y, String text, alignment align) {
  char * data  = const_cast<char*>(text.c_str());
  int  x1, y1;
  int w, h;
  int xx = x, yy = y;
  get_text_bounds(&newfont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  int cursor_y = y + h;
  write_string(&newfont, data, &x, &cursor_y, B);
}