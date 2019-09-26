// ESP 32 Cam (AI Thinker) Timelapse script
// Saves last used folder number and capture interval in a config file on the SD Card, On bootup it will use a different folder to last time to avoid overwriting files.
//
// Connect to the camera using a BLE serial app to control it
// Set the capture interval using int for example "int 3" for an interval of 3 seconds
// Send "toggle" to stop/start the capture
// Send "sdinfo" to view SD card space info
// Send "svideo" to take a picture as fast as possible, will need to stop capture by removing power as it doesnt have time to read the serial input when in this mode

#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h" //sd card esp32
#include "SD_MMC.h" //sd card esp32
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "esp32-hal-cpu.h"

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
#define SDA_pin            3
#define SCL_pin            1

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

int capture_interval;
int folderNumber = 0;

boolean NoSD = true;
bool flash = false;
String fileName;
long current_millis;
long last_capture_millis = 0;
String message = "";
bool capture = true;
bool video = false;
int fileNumber = 1;
const char *CFGfile = "/config.txt";  // <- SD library uses 8.3 CFGfiles
String response;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

// Loads the configuration from a file
void loadConfiguration() {
  // Open file for reading
  File file = SD_MMC.open(CFGfile);

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonBuffer<512> jsonBuffer;

  // Parse the root object
  JsonObject &root = jsonBuffer.parseObject(file);

  // Copy values from the JsonObject to the Config
  folderNumber = root["fnum"];
  capture_interval = root["interval"];

  if (!root.success()) {
    capture_interval = 1000;
    folderNumber = 0 ;
    Serial.println(F("Failed to read file, using default configuration"));
  }

  // Close the file (File's destructor doesn't close the file)
  file.close();
}

// Saves the configuration to a file 
void saveConfiguration() {
  // Delete existing file, otherwise the configuration is appended to the file
  SD_MMC.remove(CFGfile);

  // Open file for writing
  File file = SD_MMC.open(CFGfile, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate the memory pool on the stack
  // Don't forget to change the capacity to match your JSON document.
  // Use https://arduinojson.org/assistant/ to compute the capacity.
  StaticJsonBuffer<256> jsonBuffer;

  // Parse the root object
  JsonObject &root = jsonBuffer.createObject();

  // Set the values
  root["fnum"] = folderNumber;
  root["interval"] = capture_interval;

  // Serialize JSON to file
  if (root.printTo(file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file (File's destructor doesn't close the file)
  file.close();
}

void createDir(fs::FS &fs, String path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      String message = pCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        //Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++) { 
          //Serial.println(rxValue[i]); 
          }
        // Do stuff based on the command received from the app
        if (rxValue.find("int") != -1) {
        message.remove(0,3);
        int s2ms = message.toInt()*1000;
        capture_interval = s2ms;
        saveConfiguration();
        Serial.println("Interval set to: " + String(message.toInt()) + " Seconds (" + String(s2ms) + " MS)");
        response = "Interval set to: " + String(message.toInt()) + " Seconds (" + String(s2ms) + " MS)";
        } else if (rxValue.find("toggle") != -1) {
        if (capture){
          capture = false;
          Serial.println("Capture set to false");
          response = "Capture set to false";
        } else {
          capture = true;
          Serial.println("Capture set to true");
          response = "Capture set to true";
        }
        Serial.println("*********"); 
        } else if (rxValue.find("sdinfo") != -1) {
            //int cardSize = SD_MMC.cardSize() / (1024 * 1024);
            int totalByte = SD_MMC.totalBytes() / (1024 * 1024);
            int usedByte = SD_MMC.usedBytes() / (1024 * 1024);
            //Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
            Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
            Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

            //response = "SD_MMC Card Size: "+ String(cardSize) +"MB ";
            response += "Total space: "+ String(totalByte) +"MB - Used space: "+ String(usedByte) +"MB\n";
          
        } else if (rxValue.find("svideo") != -1) {
            video = true;
        }
        
        } } }; 

void setup() {
  setCpuFrequencyMhz(80); //Set CPU clock to 80MHz as we dont need it running at full speed
  Serial.begin(115200);
  ConfigCam();
  Serial.println("Camera ready");
  ConfigSD();
  if (NoSD == false) {
    Serial.println("SD ready");
    fileNumber = 1;
  }
  else {
    while (1 == 1) {}
  }
  
        BLEDevice::init("ESP32 Cam"); // Give it a name // Create the BLE Server 
        BLEServer *pServer = BLEDevice::createServer(); pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  // Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
  loadConfiguration();

  folderNumber += 1;

  // Create configuration file
  Serial.println(F("Saving configuration..."));
  saveConfiguration();

  createDir(SD_MMC, "/" + String(folderNumber));

}

void loop() {
    current_millis = millis();

    if (video) {
      ImageToSd();
    } else if (current_millis - last_capture_millis > capture_interval) { // Take another picture
      last_capture_millis = millis();
      if (capture) {
        ImageToSd();
        //Serial.print(getCpuFrequencyMhz());
      }
    }

   if (deviceConnected) {
    if (response.length() > 0){
    response += " \n";
    char charBuf[50];
    response.toCharArray(charBuf, 50);
    pCharacteristic->setValue(charBuf);
    pCharacteristic->notify();
    response = "";
    }
  }
}

void ConfigCam() {
  pinMode(XCLK_GPIO_NUM, OUTPUT); //otherwise pin 0 which is XCLK_GPIO_NUM for camera cannot be used
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
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }
}

void ConfigSD() {
  bool sdcardenable = true ;
  if (!SD_MMC.begin() and sdcardenable ) {
    Serial.println("SD Mount Fail");
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card");
    NoSD = true;
  }
  else {
    NoSD = false;
  }
}

void ImageToSd()
{
  if (!SD_MMC.begin()) {
    Serial.println("sdcard gone?");
  }
  else {
    fileName = "/" + String(folderNumber) + "/" + String(fileNumber) + ".jpg";
    fileNumber += 1;
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get(); //take picture from cam
    if (!fb) {
      Serial.println("Fail get picture");
      return ;
    }
    fs::FS &fs = SD_MMC;
    File file = fs.open(fileName, FILE_WRITE);
    if (!file) {
      Serial.println("Fail save picture");
    }
    else
    {
      file.write(fb->buf , fb->len);
      file.close();
    }
    esp_camera_fb_return(fb);//free buffer
    Serial.println("Image saved");
    response = "Image saved";
  }
}
