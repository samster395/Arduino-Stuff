#include "Arduino.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h> 
#include <ESP32_FTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <Stepper2.h>
#include <EEPROM.h>            // read and write from flash memory

#define WIFI_SSID ""
#define WIFI_PASS ""
bool WWF = true; // Spin turntable when finished? Will require you to manually stop the process using the web interface to stop the turntable
char *OTAHostname = "ESP32 Scan";
char *OTApass = "scanme";
char ftp_server[] = "192.168.0.3";
char ftp_user[]   = "main";
char ftp_pass[]   = "bob";

WebServer server(80);
const int rpm = 15; // max rpm on 28BYJ-48 is ~15
int pinOut[4] = { 2, 14, 15, 13 };
const int stepsScale = 2400;
int noSteps=180;
// define the number of bytes you want to access
#define EEPROM_SIZE 1
Stepper2 myStepper(pinOut);

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

int folderNumber = 0;
bool ProcRunning = false;
int pictureNumber = 0;

// you can pass a FTP timeout and debbug mode on the last 2 arguments
ESP32_FTPClient ftp (ftp_server,ftp_user,ftp_pass, 5000, 2);

void setup()
{
  Serial.begin( 115200 );
  myStepper.setSpeed(rpm);

  WiFi.begin( WIFI_SSID, WIFI_PASS );
  
  Serial.println("Connecting Wifi...");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(OTAHostname);
  ArduinoOTA.setPassword(OTApass);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  server.on("/", handle_OnConnect);
  server.on("/startproc", handle_startproc);
  server.on("/stopproc", handle_stopproc);
  server.on("/rstfc", handle_rstfc);
  server.on("/capture", handle_capture);
  server.on("/STPT", handle_STPT);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  //startProc();
}

bool startProc(){
  ftp.OpenConnection();
  EEPROM.begin(EEPROM_SIZE);
  folderNumber = EEPROM.read(0) + 1;
  ftp.InitFile("Type A");
  String path = "/scan/" + String(folderNumber);
  const char *temp = path.c_str();
  ftp.MakeDir( temp );
  ftp.CloseConnection();
  EEPROM.write(0, folderNumber);
  EEPROM.commit();
  ProcRunning = true;
  pictureNumber = 0;
}

bool stopProc()
{
    ProcRunning = false;
    pictureNumber = 0;
}

void take_photo(){
  if (ProcRunning){
  if (pictureNumber >= 181) {
    Serial.println("Finished");
    if (WWF){
    myStepper.setDirection(0); // clock-wise
    myStepper.turn();         
    myStepper.stop();
    delay(5000);
    } else {
      stopProc();
    }
  } else {
  Serial.println("------");
  ftp.OpenConnection();

  camera_fb_t * fb = NULL;
  // Take Picture with Camera
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  Serial.println("taking image");
  //debugI("* This is a message of debug level INFO");
  // Create the new file and send the image
  ftp.InitFile("Type I");
  String path = "/scan/" + String(folderNumber) + "/" + String(pictureNumber) + ".jpg";
  Serial.println(path);
  const char *temp = path.c_str();
  Serial.println("uploading to FTP");
  ftp.NewFile( temp );
  ftp.WriteData( fb->buf, fb->len );
  pictureNumber++;
  ftp.CloseFile();
  esp_camera_fb_return(fb); 
  ftp.CloseConnection();
  Serial.println("advancing stepper");
  myStepper.setDirection(0); // clock-wise
  myStepper.step(4.4*stepsScale/noSteps);
  myStepper.stop();
  delay(2000); // Can maybe be shortened, not really worried about speed though
  }
  }
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  take_photo();
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML()); 
}

void handle_startproc() {
  startProc();
  server.send(200, "text/html", "Starting"); 
}

void handle_stopproc() {
  stopProc();
  server.send(200, "text/html", "Stopping"); 
}

void handle_rstfc() {
  if (ProcRunning){
  server.send(200, "text/html", "Stop the procces first"); 
  } else {
    folderNumber = 0;
    EEPROM.write(0, folderNumber);
    EEPROM.commit();
    server.send(200, "text/html", "folder count reset"); 
  }
}

void handle_STPT() {
  myStepper.setDirection(0); // clock-wise
  myStepper.step(4.4*stepsScale/noSteps);
  myStepper.stop();
  myStepper.step(4.4*stepsScale/noSteps);
  myStepper.stop();
  server.send(200, "text/html", "testing stepper");
}

void handle_capture() {
  ftp.OpenConnection();
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  Serial.println("taking image");
  // Create the new file and send the image
  ftp.InitFile("Type I");
  String path = "/scan/test.jpg";
  Serial.println(path);
  const char *temp = path.c_str();
  Serial.println("uploading to FTP");
  ftp.NewFile( temp );
  ftp.WriteData( fb->buf, fb->len );
  ftp.CloseFile();
  esp_camera_fb_return(fb); 
  ftp.CloseConnection();
  server.send(200, "text/html", "Check FTP Folder for test.jpg"); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>ESP32 Cam - 3D scanner</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{background-color: #000000; color: #ffffff ; margin-top: 50px;} h1 {margin: 50px auto 30px;}}\n";
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>ESP32 Cam - 3D scanner</h1>\n";
  
   if(ProcRunning) {
  {ptr +="<h2>Process: Running</h2>\n";}
  {ptr +="<button id='stop' type='button' style='width: 100px; height: 50px;'>Stop</button>\n";}
  {ptr +="<script> document.addEventListener('DOMContentLoaded', function(event) { document.getElementById('stop').addEventListener('click', function(){sendCommand('stopproc', true);}, false);";}
   } else {
  {ptr +="<h2>Process: Not Running</h2>\n";}
  {ptr +="<button id='start' type='button' style='width: 100px; height: 50px;'>Start</button><br><br><br>\n";}
  {ptr +="<button id='cap' type='button' >Save test image to FTP</button>\n";}
  {ptr +="<button id='STPT' type='button' >Test stepper movement</button>\n";}
  {ptr +="<button id='rstfc' type='button' >Reset folder count</button>\n";}
  {ptr +="<script> document.addEventListener('DOMContentLoaded', function(event) { document.getElementById('start').addEventListener('click', function(){sendCommand('startproc', true);}, false); document.getElementById('cap').addEventListener('click', function(){sendCommand('capture');}, false); document.getElementById('STPT').addEventListener('click', function(){sendCommand('STPT');}, false); document.getElementById('rstfc').addEventListener('click', function(){sendCommand('rstfc');}, false);";}
   }

  ptr += "function sendCommand(cmd, r) { var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() {if (this.readyState == 4 && this.status == 200) {}};xhttp.open('GET', window.location.href + cmd, true);xhttp.send(); if(r){ setTimeout(function(){ location.reload(); }, 2500);} } }); </script>";
  ptr +="<p>Give it a little while to respond to the command</p>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}
