#include <Arduino.h>
#include <ESP32Servo.h>
#include <Bluepad32.h>
#include "Cdrv8833.h"
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
//#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <iostream>
#include <sstream>
#include <Adafruit_NeoPixel.h>

//#include <Battery.h>

//Battery battery(6800, 8400, 36);

bool batteryMonitoring = true; // Uses a 22k and 10k resistor voltage divider connected to GPIO 36/VP pin to bring the battery pack voltage(8.4v fully charged) below 3.3v for the ESP's ADC pins.
bool UseWifiManager = true; // You can either use WiFiManager - https://github.com/tzapu/WiFiManager or set this to false and input ssid and pass below
// If its not moving the right way you can change these to fix it rather than rewiring
bool leftMotorswap = false; 
bool rightMotorswap = true;

const char* ssid = "";
const char* password = "";

#define PIN 1
#define LED_COUNT 2
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRBW + NEO_KHZ800); // sk6812 

WiFiManager wifiManager;

AsyncWebServer server(80);
AsyncWebSocket wsCarInput("/CarInput");

unsigned long ota_progress_millis = 0;

extern const char* htmlHomePage PROGMEM;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

#define rightMotor0 25
#define rightMotor0Dir HIGH
#define rightMotor1 26
#define rightMotor1Dir HIGH

#define leftMotor0 33
#define leftMotor0Dir HIGH
#define leftMotor1 32
#define leftMotor1Dir HIGH

#define armMotor0 21
#define armMotor0Dir HIGH
#define armMotor1 19
#define armMotor1Dir HIGH

#define bucketServoPin 23
#define clawServoPin 22

#define auxLights0 16
#define auxLights1 17

int8_t rightMotorReverse = 1;
int8_t leftMotorReverse = 1;
int8_t armMotorReverse = 1;
Cdrv8833 rightMotor;
Cdrv8833 leftMotor;
Cdrv8833 armMotor;

Servo bucketServo;
Servo clawServo;

volatile int bucketServoDelay = 7;
volatile unsigned long bucketServoLastMove = 0;
volatile int bucketServoMax = 170;
volatile int bucketServoMin = 10;
volatile int bucketServoValue = bucketServoMax;

volatile int clawServoDelay = 7;
volatile unsigned long clawServoLastMove = 0;
volatile int clawServoMax = 140;
volatile int clawServoMin = 10;
volatile int clawServoValue = clawServoMax;

volatile bool auxLightsOn = true;
volatile int moveClawServoSpeed = 0;
volatile int moveBucketServoSpeed = 0;

volatile unsigned long lastWiggleTime = 0;
volatile int wiggleCount = 0;
volatile int wiggleDirection = 1;
unsigned long wiggleDelay = 100;
volatile bool shouldWiggle = false;

ControllerPtr controller;

// This callback gets called any time a new gamepad is connected.
void onConnectedController(ControllerPtr ctl) {
  if (controller == nullptr) {
    Serial.printf("CALLBACK: Controller is connected");
    ControllerProperties properties = ctl->getProperties();
    Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName().c_str(), properties.vendor_id, properties.product_id);
    controller = ctl;
    ctl->setColorLED(255, 0, 0);
    shouldWiggle = true;
    ctl->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */, 0x40 /* strongMagnitude */);
  } else {
    Serial.println("CALLBACK: Controller connected, but could not found empty slot");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (controller == ctl) {
    Serial.printf("CALLBACK: Controller disconnected");
    controller = nullptr;
  } else {
    Serial.println("CALLBACK: Controller disconnected, but not found in myControllers");
  }
}

void processGamepad(ControllerPtr ctl) {
  int LXValue = ctl->axisX();
  int LYValue = ctl->axisY();
  int8_t driveInput = -map(LYValue, -512, 511, -100, 100);
  int8_t steeringInput = map(LXValue, -512, 511, -100, 100);

  //Serial.print("Drive: ");
  //Serial.println(driveInput);

  //Serial.print("Steer: ");
  //Serial.println(steeringInput);

  int8_t leftMotorSpeed = max(min(driveInput - steeringInput, 100), -100);
  int8_t rightMotorSpeed = max(min(driveInput + steeringInput, 100), -100);
  
  leftMotor.move(leftMotorSpeed);
  rightMotor.move(rightMotorSpeed);
  if (LXValue > -2 && LXValue < 2 && LYValue > -2 && LYValue < 2) {
    // Stick centered, stop movement
    rightMotor.stop();
    leftMotor.stop();
  }

  int RXValue = (ctl->axisRX());
  int RYValue = (ctl->axisRY());
  if (abs(RXValue) + abs(RYValue) > 2) {
    int8_t armSpeed = map(RYValue, -512, 511, -100, 100);
    armMotor.move(armSpeed);
  }
  if (RYValue > -30 && RYValue < 30) {
    armMotor.stop();
  }

  // Check shoulder to move claw
  if (ctl->l1() && ctl->r1() || !ctl->l1() && !ctl->r1()) {
    moveClawServoSpeed = 0;
  }
  if (ctl->l1()) {
    moveClawServoSpeed = 1;
  }
  if (ctl->r1()) {
    moveClawServoSpeed = -1;
  }

  // Check throttle to move bucket
  if (ctl->l2() && ctl->r2() || !ctl->l2() && !ctl->r2()) {
    moveBucketServoSpeed = 0;
  }
  if (ctl->l2()) {
    moveBucketServoSpeed = 1;
  }
  if (ctl->r2()) {
    moveBucketServoSpeed = -1;
  }

  if (ctl->a()) {
    shouldWiggle = true;
  }

  if (ctl->thumbR()) {
    if (!auxLightsOn) {
      colorSet(strip.Color(255, 255, 255, 0), 255); 
      auxLightsOn = true;
    } else {
      colorSet(strip.Color(0, 0, 0, 0), 255); 
      auxLightsOn = false;
    }
  }
}

void processControllers() {
  if (controller && controller->isConnected() && controller->hasData()) {
    if (controller->isGamepad()) {
      processGamepad(controller);
    } else {
      Serial.println("Unsupported controller");
    }
  }
}

void bucketTilt(int bucketServoValue)
{
  bucketServo.write(bucketServoValue);
}
void auxControl(int auxServoValue)
{
  clawServo.write(auxServoValue);
}

void notifyClientsBattery() {
  #define BATTV_MAX    8.4     // maximum voltage of battery
  #define BATTV_MIN    6.4     // what we regard as an empty battery
  #define BATTV_LOW    6.8     // voltage considered to be low battery
  //wsCarInput.textAll(battery.level() + battery.voltage() + "%");
  //wsCarInput.printfAll("Battery Voltage: %u", battery.voltage());
  //wsCarInput.printfAll("%u%", battery.level());
  //wsCarInput.printfAll("%u", analogRead(36));
  float battv = ((float)analogRead(36) / 4095) * 3.3 * 3.2 * 1.07;
  float battv2 = ((float)analogRead(36) / 4095) * 3.3 * 3.2 * 1.07 * 1000;
  float battpc = (uint8_t)(((battv - BATTV_MIN) / (BATTV_MAX - BATTV_MIN)) * 100);
  if(battpc > 100) { battpc = 100;}
  wsCarInput.textAll(String(battv, 2) + "V");
  wsCarInput.textAll(String(battpc, 0) + "%");
  wsCarInput.textAll(String(battv2, 2) + "mV");
  //Serial.print("Battery voltage is ");
	//Serial.println(battery.voltage());
  //Serial.print("Analod Read: ");
  //Serial.println(analogRead(36));
  //Serial.print("Analog Read with formula: ");
  //Serial.println((analogRead(36) / 4095) * 3.3 * 3.2);
}

void colorSet(uint32_t c, uint8_t brightness) { // From NeoPixel Library
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.setBrightness(brightness);
  strip.show();
}

void onCarInputWebSocketEvent(AsyncWebSocket *server,
                              AsyncWebSocketClient *client,
                              AwsEventType type,
                              void *arg,
                              uint8_t *data,
                              size_t len)
{
  switch (type)
  {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      //moveCar(STOP);
      break;
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
      {
        std::string myData = "";
        myData.assign((char *)data, len);
        std::istringstream ss(myData);
        std::string key, value, value2, value3, value4, value5;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        std::getline(ss, value2, ',');
        std::getline(ss, value3, ',');
        std::getline(ss, value4, ',');
        std::getline(ss, value5, ',');
        //Serial.printf("Key [%s] Value[%s]\n", key.c_str(), value.c_str());
        Serial.printf("Key [%s] Value[%s] Value2[%s]\n", key.c_str(), value.c_str(), value2.c_str());
        int valueInt = atoi(value.c_str());
        int valueInt2 = atoi(value2.c_str());
        int valueInt3 = atoi(value3.c_str());
        int valueInt4 = atoi(value4.c_str());
        int valueInt5 = atoi(value5.c_str());
        if (key == "MoveCar")
        {
          //moveCar(valueInt);
          Serial.printf("Move Car: [%s]\n", value.c_str());
        }
        else if (key == "LXY")
        {
          //Serial.printf("LXY is [%s]\n", value.c_str());
          Serial.printf("LXY - X:[%s] Y:[%s]\n", value.c_str(), value2.c_str());
      
          int8_t driveInput = valueInt;
          int8_t steeringInput = valueInt2;

          //int8_t driveInput = -map(LYValue, -512, 511, -100, 100);
          //int8_t steeringInput = map(LXValue, -512, 511, -100, 100);

          int8_t leftMotorSpeed = max(min(driveInput - steeringInput, 100), -100);
          int8_t rightMotorSpeed = max(min(driveInput + steeringInput, 100), -100);

          leftMotor.move(leftMotorSpeed);
          rightMotor.move(rightMotorSpeed);
          if (driveInput > -2 && driveInput < 2 && steeringInput > -2 && steeringInput < 2) {
            // Stick centered, stop movement
            rightMotor.stop();
            leftMotor.stop();
          }
        }
        else if (key == "RXY")
        {
          //Serial.printf("LXY is [%s]\n", value.c_str());
          Serial.printf("RXY - X:[%s] Y:[%s]\n", value.c_str(), value2.c_str());

          int RXValue = valueInt;
          int RYValue = valueInt2;
          if (abs(RXValue) + abs(RYValue) > 2) {
            //int8_t armSpeed = map(RYValue, -512, 511, -100, 100);
            if(RYValue < 0) { RYValue = RYValue + (abs(RYValue)*2); } else if (RYValue > 0) { RYValue = RYValue - (abs(RYValue)*2); }
            Serial.println(RYValue);
            armMotor.move(RYValue);
          }
          if (RYValue > -30 && RYValue < 30) {
            armMotor.stop();
          }
        }
        else if (key == "AUX")
        {
          auxControl(valueInt);
          Serial.printf("auxControl: [%s]\n", value.c_str());
        }
        else if (key == "Bucket")
        {
          bucketTilt(valueInt);
          Serial.printf("bucketTilt: [%s]\n", value.c_str());
        }
        else if (key == "Light")
        {
          colorSet(strip.Color(valueInt, valueInt2, valueInt3, valueInt4), valueInt5); 
          Serial.printf("Light - R:[%s] G:[%s] B:[%s] W:[%s] Bri:[%s]\n", value.c_str(), value2.c_str(), value3.c_str(), value4.c_str(), value5.c_str());
        }
        else if (key == "Wiggle")
        {
          shouldWiggle = true;
        }
        else if (key == "Switch")
        {
          //if (!(horizontalScreen))
          //{
          //  horizontalScreen = true;
          //}
          //else {
          //  horizontalScreen = false;
          //}
        }
        //notifyClients();
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;
  }
}

void setup() {
  //battery.begin(3300, 3.2);
  //battery.begin(3300, 3.2, &sigmoidal);
  Serial.begin(115200);
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t* addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  strip.begin();
  colorSet(strip.Color(255, 255, 255, 0), 0); // Cold WHITE
  strip.show(); // Initialize all pixels to Cold WHITE

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();
  BP32.enableVirtualDevice(false);

  Serial.println("Ready.");

  rightMotor.init(rightMotor0, rightMotor1, 5);
  rightMotor.setDecayMode(drv8833DecaySlow);
  rightMotor.swapDirection(rightMotorswap);
  leftMotor.init(leftMotor0, leftMotor1, 6);
  leftMotor.setDecayMode(drv8833DecaySlow);
  leftMotor.swapDirection(leftMotorswap);
  armMotor.init(armMotor0, armMotor1, 7);
  armMotor.setDecayMode(drv8833DecaySlow);

  pinMode(auxLights0, OUTPUT);
  pinMode(auxLights1, OUTPUT);
  digitalWrite(auxLights0, HIGH);
  digitalWrite(auxLights1, HIGH);

  bucketServo.attach(bucketServoPin);
  clawServo.attach(clawServoPin);

  bucketServo.write(bucketServoValue);
  clawServo.write(clawServoValue);

  if(UseWifiManager) {
    wifiManager.autoConnect("MiniSkidi");
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");
  }


  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //request->send(200, "text/plain", "Hi! This is ElegantOTA AsyncDemo.");
    request->send_P(200, "text/html", htmlHomePage);
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);


  wsCarInput.onEvent(onCarInputWebSocketEvent);
  server.addHandler(&wsCarInput);

  server.begin();
  Serial.println("HTTP server started");
}

void wiggle() {
  unsigned long currentTime = millis();
  if (abs((int)(currentTime - lastWiggleTime)) >= wiggleDelay) {
    lastWiggleTime = currentTime;
    wiggleDirection = -wiggleDirection;
    wiggleCount++;
    rightMotor.move(wiggleDirection * 100);
    leftMotor.move(-1 * wiggleDirection * 100);
    if (wiggleCount >= 10) {
      rightMotor.brake();
      leftMotor.brake();
      wiggleCount = 0;
      shouldWiggle = false;
    }
  }
}

//int period = 5000; // MS - 5s
int period = 15000; // MS - 15s
//int period = 30000; // MS - 30s
unsigned long time_now = 0;

void loop() {

  if(batteryMonitoring){
  if(millis() >= time_now + period){
    time_now += period;
    notifyClientsBattery();
  }
  }

  unsigned long currentTime = millis();
  bool dataUpdated = BP32.update();
  if (dataUpdated) {
    processControllers();
  }
  if (shouldWiggle) {
    wiggle();
  }
  if (moveClawServoSpeed != 0) {
    if (currentTime - clawServoLastMove >= clawServoDelay) {
      clawServoLastMove = currentTime;
      int newClawServoValue = clawServoValue + moveClawServoSpeed;
      if (newClawServoValue >= clawServoMin && newClawServoValue <= clawServoMax) {
        clawServoValue = newClawServoValue;
        clawServo.write(clawServoValue);
      }
    }
  }
  if (moveBucketServoSpeed != 0) {
    if (currentTime - bucketServoLastMove > bucketServoDelay) {
      bucketServoLastMove = currentTime;
      int newBucketServoValue = bucketServoValue + moveBucketServoSpeed;
      if (newBucketServoValue >= bucketServoMin && newBucketServoValue <= bucketServoMax) {
        bucketServoValue = newBucketServoValue;
        bucketServo.write(bucketServoValue);
      }
    }
  }

  ElegantOTA.loop();

  wsCarInput.cleanupClients();

}