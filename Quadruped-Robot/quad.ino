//==========================================================================================
// nakanoshima robot:○ kawa　2018.5.5
// mePed_nakarobo_COM.ino 
//
// The mePed is an open source quadruped robot designed by Scott Pierce of 
// Spierce Technologies (www.meped.io & www.spiercetech.com) 
// 
// This program is based on code written by Alexey Butov (www.alexeybutov.wix.com/roboset) 
// 
//========================================================================================== 
#include <ESP8266WiFi.h> 
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Servo.h> 
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "RemoteDebug.h"

// RUN
// Open serial monitor 9600bps and Input numeric.
//
// 0 minimal
// 1 lean_left
// 2 forward
// 3 lean_right
// 4 turn_right
// 5 center_servos
// 6 turn_left
// 7 step_left
// 8 back
// 9 step_right

//Quad (Top View)
//FRONT
//  -----               -----
// |  2  |             |  8  |
// | P03 |             | P09 |
//  ----- -----   ----- -----
//       |  1  | |  7  |
//       | P02 | | P08 |
//        -----   -----
//       |  3  | |  5  |
//       | P04 | | P06 |
//  ----- -----   ----- -----
// |  4  |             |  6  |
// | P05 |             | P07 |
//  -----               -----  
//BACK

//Arudiroid4 (Top View)
//FLONT
// sp1: Speed 1 sp2: Speed 2 sp3: Speed 3 sp4: Speed 4 
//
//  -----       -----
// | SP1 |      | SP4 |
//  -----        -----
//        ----- 
//       | Nano|
//       |     |
//        ----- 
//  -----        -----
// | SP2 |      | SP3 |
//  -----        -----  
//BACK

// a,90,90,90,90,90,90,90,90 [Enter]
// s,1,90 [Enter]
  
//===== Globals ============================================================================ 
/* define port */
//WiFiClient client;
ESP8266WebServer server(80);
RemoteDebug Debug;

char* Hostname = "Quad"; // The network name
char* otaPass = "quadbot"; // The network name

#define MAX_SERVO_NUM 8
Servo servo[MAX_SERVO_NUM+1];

/* data received from application */
String  data =""; 
// calibration
int da = -12; // Left Front Pivot 
int db =  18; // Left Back Pivot
int dc = -18; // Right Back Pivot
int dd =  12; // Right Front Pivot 
// servo initial positions + calibration 
int a90 = (90 + da), a120 = (120 + da), a150 = (150 + da), a180 = (180 + da);
int b0 = (0 + db), b30 = (30 + db), b60 = (60 + db), b90 = (90 + db);
int c90 = (90 + dc), c120 = (120 + dc), c150 = (150 + dc), c180 = (180 + dc);
int d0 = (0 + dd), d30 = (30 + dd), d60 = (60 + dd), d90 = (90 + dd);
// start points for servo 
int s11 = 90; // Front Left Pivot Servo 
int s12 = 90; // Front Left Lift Servo 
int s21 = 90; // Back Left Pivot Servo 
int s22 = 90; // Back Left Lift Servo 
int s31 = 90; // Back Right Pivot Servo 
int se32 = 90; // Back Right Lift Servo 
int s41 = 90; // Front Right Pivot Servo
int s42 = 90; // Front Right Lift Servo
int f = 0;
int b = 0;
int l = 0;
int r = 0;
int spd = 50;// Speed of walking motion, larger the number, the slower the speed
int high = 0;// How high the robot is standing

// Define 8 Servos 
//Servo servo[1]; // Front Left Pivot S  ervo 
//Servo servo[2]; // Front Left Lift Servo 
//Servo servo[3]; // Back Left Pivot Servo 
//Servo servo[4]; // Back Left Lift Servo 
//Servo servo[5]; // Back Right Pivot Servo 
//Servo servo[6]; // Back Right Lift Servo 
//Servo servo[7]; // Front Right Pivot Servo 
//Servo servo[8]; // Front Right Lift Servo 

const char MAIN_page[] PROGMEM = R"=====(
  <!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
body {
  background: #2c3e50ff;
  color: red;
  text-align: center; 
  font-family: "Trebuchet MS", Arial; 
  margin-left:auto; 
  margin-right:auto;
}
button:focus {
    outline: 0;
}
a:visited {
    color: green;
}

a:link {
    color: red;
}
</style>
<title>Quad Control</title>
<body>
</head><body>
<script>
function SendCommand(Command) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
  };
  xhttp.open("GET", "/api?command=" + Command + "", true);
  xhttp.send();
}

function toggleElement(divID) {
  var x = document.getElementById(divID);
  if (x.style.display === "none") {
    x.style.display = "block";
  } else {
    x.style.display = "none";
  }
}
</script>
<h1>Quad Control</h1>
<button onclick="SendCommand('stop')">Home</button> | <button onclick="SendCommand('minimal')">Minimal</button><br><br>

<button onclick="SendCommand('forward')">Forward</button><br>
<button onclick="SendCommand('left')">Left</button><button onclick="SendCommand('right')">Right</button><br>
<button onclick="SendCommand('backward')">Back</button><br><br>

<button onclick="SendCommand('stepl')">Step Left</button> | <button onclick="SendCommand('stepr')">Step Right</button><br><br>
<button onclick="SendCommand('leanl')">Lean Left</button> | <button onclick="SendCommand('leanr')">Lean Right</button><br><br>
<button onclick="SendCommand('lay')">Lay</button> | <button onclick="SendCommand('s2s')">Side to Side</button><br><br>
<button onclick="SendCommand('fightst')">Fighting Stance</button>  | <button onclick="SendCommand('bow')">bow</button><br><br>

<button onclick="toggleElement('serC')">Toggle Servo Control</button>
<div id="serC" style="display: none;">
<img style="position: absolute; left: 10px; top: 550px;" src='data:image/png;base64,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' alt=''>
<p>Servo 1: <span id="servoPos1">104</span></p>
<input type="range" min="0" max="180" class="slider" id="s1" onchange="servo1(this.value)" value="90">
<p>Servo 2: <span id="servoPos2">105</span></p>
<input type="range" min="0" max="180" class="slider" id="s2" onchange="servo2(this.value)" value="90">

<p>Servo 3: <span id="servoPos3">104</span></p>
<input type="range" min="0" max="180" class="slider" id="s3" onchange="servo3(this.value)" value="90">
<p>Servo 4: <span id="servoPos4">105</span></p>
<input type="range" min="0" max="180" class="slider" id="s4" onchange="servo4(this.value)" value="90">

<p>Servo 5: <span id="servoPos5">104</span></p>
<input type="range" min="0" max="180" class="slider" id="s5" onchange="servo5(this.value)" value="90">
<p>Servo 6: <span id="servoPos6">105</span></p>
<input type="range" min="0" max="180" class="slider" id="s6" onchange="servo6(this.value)" value="90">

<p>Servo 7: <span id="servoPos7">104</span></p>
<input type="range" min="0" max="180" class="slider" id="s7" onchange="servo7(this.value)" value="90">
<p>Servo 8: <span id="servoPos8">105</span></p>
<input type="range" min="0" max="180" class="slider" id="s8" onchange="servo8(this.value)" value="90">

<script>
      var slider = document.getElementById("s1");
            var servoP = document.getElementById("servoPos1"); 
      servoP.innerHTML = slider.value;
            slider.oninput = function() { slider.value = this.value; servoP.innerHTML = this.value; }
            function servo1(pos) {
            SendCommand('s,1,'+ pos);
            }
      var slider2 = document.getElementById("s2");
            var servoP2 = document.getElementById("servoPos2"); 
            servoP2.innerHTML = slider2.value;
            slider2.oninput = function() { slider2.value = this.value; servoP2.innerHTML = this.value; }
            function servo2(pos) {
            SendCommand('s,2,'+ pos);
            }
      var slider3 = document.getElementById("s3");
            var servoP3 = document.getElementById("servoPos3"); 
            servoP3.innerHTML = slider3.value;
            slider3.oninput = function() { slider3.value = this.value; servoP3.innerHTML = this.value; }
            function servo3(pos) {
            SendCommand('s,3,'+ pos);
            }
      var slider4 = document.getElementById("s4");
            var servoP4 = document.getElementById("servoPos4"); 
            servoP4.innerHTML = slider4.value;
            slider4.oninput = function() { slider4.value = this.value; servoP4.innerHTML = this.value; }
            function servo4(pos) {
            SendCommand('s,4,'+ pos);
            }
      var slider5 = document.getElementById("s5");
            var servoP5 = document.getElementById("servoPos5"); 
            servoP5.innerHTML = slider5.value;
            slider5.oninput = function() { slider5.value = this.value; servoP5.innerHTML = this.value; }
            function servo5(pos) {
            SendCommand('s,5,'+ pos);
            }
      var slider6 = document.getElementById("s6");
            var servoP6 = document.getElementById("servoPos6"); 
            servoP6.innerHTML = slider6.value;
            slider6.oninput = function() { slider6.value = this.value; servoP6.innerHTML = this.value; }
            function servo6(pos) {
            SendCommand('s,6,'+ pos);
            }
      var slider7 = document.getElementById("s7");
            var servoP7 = document.getElementById("servoPos7"); 
            servoP7.innerHTML = slider7.value;
            slider7.oninput = function() { slider7.value = this.value; servoP7.innerHTML = this.value; }
            function servo7(pos) {
            SendCommand('s,7,'+ pos);
            }
  var slider8 = document.getElementById("s8");
            var servoP8 = document.getElementById("servoPos8"); 
            servoP8.innerHTML = slider8.value;
            slider8.oninput = function() { slider8.value = this.value; servoP8.innerHTML = this.value; }
            function servo8(pos) {
            SendCommand('s,8,'+ pos);
            }
</script>

</body>
</html>
)=====";

void handleRoot() {
 String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}

void handleSpecificArg() { 

String message = "Invalid or No command given";

if (server.arg("command")== ""){    //Parameter not found

}else{     //Parameter found

if (server.arg("command") == "forward"){
  forward();
  message = "Received";
}
if (server.arg("command") == "backward"){
  back();
  message = "Received";
}
if (server.arg("command") == "left"){
  turn_left();
  message = "Received";
}
if (server.arg("command") == "right"){
  turn_right();
  message = "Received";
}
if (server.arg("command") == "stop"){
  center_servos();
  message = "Received";
}
if (server.arg("command") == "minimal"){
  minimal();
  message = "Received";
}
if (server.arg("command") == "leanl"){
  lean_left();
  message = "Received";
}
if (server.arg("command") == "leanr"){
  lean_right();
  message = "Received";
}
if (server.arg("command") == "stepl"){
  step_left();
  message = "Received";
}
if (server.arg("command") == "stepr"){
  step_right();
  message = "Received";
}
if (server.arg("command") == "lay"){
  lay();
  message = "Received";
}
if (server.arg("command") == "s2s"){
  side2side();
  message = "Received";
}
if (server.arg("command") == "fightst"){
  fightst();
  message = "Received";
}
if (server.arg("command") == "bow"){
  bow();
  message = "Received";
}

String parseArg = getValue(server.arg("command"), ',', 0); // if  a,4,D,r  would return D  
if (parseArg == "s" || parseArg == "S"){
  int servoN = getValue(server.arg("command"), ',', 1).toInt(); // if  a,4,D,r  would return D 
  int sAngle = getValue(server.arg("command"), ',', 2).toInt(); // if  a,4,D,r  would return D
  if ( 0 <= servoN &&  servoN < MAX_SERVO_NUM+1 )
      {
        // Single Servo Move
        servo[servoN].write(sAngle);

        // Wait Servo Move
        //delay(300); // 180/60(°/100msec）=300(msec)
      }
  message = "Received";
  Debug.println("Servo moved");
}

if (parseArg == "a" || parseArg == "A"){
  int angle[MAX_SERVO_NUM];
  angle[0] = getValue(server.arg("command"), ',', 1).toInt(); // if  a,4,D,r  would return D 
  angle[1] = getValue(server.arg("command"), ',', 2).toInt(); // if  a,4,D,r  would return D 
  angle[2] = getValue(server.arg("command"), ',', 3).toInt(); // if  a,4,D,r  would return D 
  angle[3] = getValue(server.arg("command"), ',', 4).toInt(); // if  a,4,D,r  would return D 
  angle[4] = getValue(server.arg("command"), ',', 5).toInt(); // if  a,4,D,r  would return D
  angle[5] = getValue(server.arg("command"), ',', 6).toInt(); // if  a,4,D,r  would return D
  angle[6] = getValue(server.arg("command"), ',', 7).toInt(); // if  a,4,D,r  would return D
  angle[7] = getValue(server.arg("command"), ',', 8).toInt(); // if  a,4,D,r  would return D
  
  set8Pos( angle[0], angle[1], angle[2], angle[3], angle[4], angle[5], angle[6], angle[7] );
  message = "Received";
  Debug.println("Servos moved");
}



}
// String  var = getValue( StringVar, ',', 2); // if  a,4,D,r  would return D  
//server.send(200, "text/plain", message);          //Returns the HTTP response
  String page = "";
  page = "<title>Quad Control API</title><center><h2>Response: "+String(message)+"</h2><br><br>";
  page += "<img src='data:image/png;base64,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' alt=''>";
  server.send(200, "text/html", page);
}

//===== Setup ============================================================================== 
void setup() {
  Serial.begin (9600);
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

  String IPString = WiFi.localIP().toString();
  Serial.println(IPString);
  // Start position
  attachServo();
  delay(300);
//  minimal();
  center_servos();
  delay(300);
//  detachServo();
  delay(300);
  
  server.on("/", handleRoot);    
  server.on("/api", handleSpecificArg);   //Associate the handler function to the path
  
  server.begin();                                       //Start the server
  Serial.println("Server listening");   
  
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
}

//setup 
//========================================================================================== 

//== Loop ================================================================================== 
void loop() {
  unsigned long value;
  int lastValue;

  // Center all servos 
  high = 15;
  // Set hight to 15 
  spd = 3;
  // Set speed to 3 

//attachServo();
server.handleClient(); 
  ArduinoOTA.handle();
  Debug.handle();
 if (Serial.available() > 0) {

    char command = Serial.read();
    
    // SERVO Set command
    if ( command == 'S' || command == 's' )  // 'S' is Servo Command "S,Servo_no,Angle"
    {
      Serial.print(command);
      Serial.print(',');
      int servoNo = Serial.parseInt();
      Serial.print(servoNo);
      Serial.print(',');
      int servoAngle = Serial.parseInt();
      Serial.print(servoAngle);
      Serial.println();

      if ( 0 <= servoNo &&  servoNo < MAX_SERVO_NUM+1 )
      {
        // Single Servo Move
        servo[servoNo].write(servoAngle);

        // Wait Servo Move
        //delay(300); // 180/60(°/100msec）=300(msec)
      }
    }  // SERVO Set END

    // SERVO All command
    if ( command == 'A' || command == 'a' )  // 'A' is Servo All Command "A,Angle0, Angle1...Angle7"
    {
      Serial.print(command);

      // read Angle from Serial
      int angle[MAX_SERVO_NUM];
      for (int i = 0; i < MAX_SERVO_NUM; i++)
      {
        angle[i] = Serial.parseInt();
        Serial.print(',');
        Serial.print(angle[i]);
      }
      Serial.println();

      // servo Move
      set8Pos( angle[0], angle[1], angle[2], angle[3], angle[4], angle[5], angle[6], angle[7] );

    }

 //   detachServo(); 
 }
    
}//loop

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length();

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}  // END

// Set 8 ServoMotor angles
void set8Pos(int a, int b, int c, int d, int e, int f, int g, int h){
  servo[1].write(a);
  servo[2].write(b);
  servo[3].write(c);
  servo[4].write(d);
  servo[5].write(e);
  servo[6].write(f);
  servo[7].write(g);
  servo[8].write(h);
}

void attachServo()
{
  servo[1].attach(D1);
  servo[2].attach(D2);
  servo[3].attach(D3);
  servo[4].attach(D4);
  servo[5].attach(D5);
  servo[6].attach(D6);
  servo[7].attach(D7);
  servo[8].attach(D8);
}

void detachServo()
{
  servo[1].detach();
  servo[2].detach();
  servo[3].detach();
  servo[4].detach();
  servo[5].detach();
  servo[6].detach();
  servo[7].detach();
  servo[8].detach();
}

//== Lean_Left ============================================================================= 
void lean_left() {
  servo[2].write(40);
  servo[4].write(140);
  servo[6].write(140);
  servo[8].write(40);
  delay(700);
}

//== Lean_Right ============================================================================ 
void lean_right() {
  servo[2].write(140);
  servo[4].write(40);
  servo[6].write(40);
  servo[8].write(140);
  delay(700);
}

// trim
const int trimStep = 1;
//== trim_left ============================================================================= 
void trim_left() {
  da-=trimStep;
  // Left Front Pivot
  db-=trimStep;
  // Left Back Pivot
  dc-=trimStep;
  // Right Back Pivot
  dd-=trimStep;
  // Right Front Pivot
}

//== trim_Right ============================================================================ 
void trim_right() {
  da+=trimStep;
  // Left Front Pivot 
  db+=trimStep;
  // Left Back Pivot
  dc+=trimStep;
  // Right Back Pivot
  dd+=trimStep;
  // Right Front Pivot
}

//== Forward ===============================================================================
void forward() {
  // calculation of points 
  // Left Front Pivot 
  a90 = (90 + da), a120 = (120 + da), a150 = (150 + da), a180 = (180 + da);
  // Left Back Pivot
  b0 = (0 + db), b30 = (30 + db), b60 = (60 + db), b90 = (90 + db);
  // Right Back Pivot
  c90 = (90 + dc), c120 = (120 + dc), c150 = (150 + dc), c180 = (180 + dc);
  // Right Front Pivot
  d0 = (0 + dd), d30 = (30 + dd), d60 = (60 + dd), d90 = (90 + dd);
  // set servo positions and speeds needed to walk forward one step 
  // (LFP, LBP, RBP, RFP, LFL, LBL, RBL, RFL, S1, S2, S3, S4) 
  srv(a180, b0 , c120, d60, 42, 33, 33, 42, 1, 3, 1, 1);
  srv( a90, b30, c90, d30, 6, 33, 33, 42, 3, 1, 1, 1);
  srv( a90, b30, c90, d30, 42, 33, 33, 42, 3, 1, 1, 1);
  srv(a120, b60, c180, d0, 42, 33, 6, 42, 1, 1, 3, 1);
  srv(a120, b60, c180, d0, 42, 33, 33, 42, 1, 1, 3, 1);
  srv(a150, b90, c150, d90, 42, 33, 33, 6, 1, 1, 1, 3);
  srv(a150, b90, c150, d90, 42, 33, 33, 42, 1, 1, 1, 3);
  srv(a180, b0, c120, d60, 42, 6, 33, 42, 1, 3, 1, 1);
  //
  srv(a180, b0, c120, d60, 42, 15, 33, 42, 1, 3, 1, 1);
//  Serial.println("Forward");
  
}

//== Back ================================================================================== 
void back () {
  // set servo positions and speeds needed to walk backward one step 
  // (LFP, LBP, RBP, RFP, LFL, LBL, RBL, RFL, S1, S2, S3, S4) 
  srv(180, 0, 120, 60, 42, 33, 33, 42, 3, 1, 1, 1);
  srv(150, 90, 150, 90, 42, 18, 33, 42, 1, 3, 1, 1);
  srv(150, 90, 150, 90, 42, 33, 33, 42, 1, 3, 1, 1);
  srv(120, 60, 180, 0, 42, 33, 33, 6, 1, 1, 1, 3);
  srv(120, 60, 180, 0, 42, 33, 33, 42, 1, 1, 1, 3);
  srv(90, 30, 90, 30, 42, 33, 18, 42, 1, 1, 3, 1);
  srv(90, 30, 90, 30, 42, 33, 33, 42, 1, 1, 3, 1);
  //
//  srv(180, 0, 120, 60, 6, 33, 33, 42, 3, 1, 1, 1);
  srv(160,20, 120, 60, 18, 33, 33, 42, 3, 1, 1, 1);
  
}

//== Left =================================================================================
void turn_left () {
  // set servo positions and speeds needed to turn left one step 
  // (LFP, LBP, RBP, RFP, LFL, LBL, RBL, RFL, S1, S2, S3, S4) 
  srv(150, 90, 90, 30, 42, 6, 33, 42, 1, 3, 1, 1);
  srv(150, 90, 90, 30, 42, 33, 33, 42, 1, 3, 1, 1); 
  srv(120, 60, 180, 0, 42, 33, 6, 42, 1, 1, 3, 1);
  srv(120, 60, 180, 0, 42, 33, 33, 24, 1, 1, 3, 1);
  srv(90, 30, 150, 90, 42, 33, 33, 6, 1, 1, 1, 3);
  srv(90, 30, 150, 90, 42, 33, 33, 42, 1, 1, 1, 3);
  srv(180, 0, 120, 60, 6, 33, 33, 42, 3, 1, 1, 1);
  srv(180, 0, 120, 60, 42, 33, 33, 33, 3, 1, 1, 1);
  
}

//== Right ================================================================================
void turn_right () { 
  // set servo positions and speeds needed to turn right one step 
  // (LFP, LBP, RBP, RFP, LFL, LBL, RBL, RFL, S1, S2, S3, S4) 
  srv( 90, 30, 150, 90, 6, 33, 33, 42, 3, 1, 1, 1);
  srv( 90, 30, 150, 90, 42, 33, 33, 42, 3, 1, 1, 1);
  srv(120, 60, 180, 0, 42, 33, 33, 6, 1, 1, 1, 3);
  srv(120, 60, 180, 0, 42, 33, 33, 42, 1, 1, 1, 3);
  srv(150, 90, 90, 30, 42, 33, 6, 42, 1, 1, 3, 1);
  srv(150, 90, 90, 30, 42, 33, 33, 42, 1, 1, 3, 1);
  srv(180, 0, 120, 60, 42, 6, 33, 42, 1, 3, 1, 1);
  srv(180, 0, 120, 60, 42, 33, 33, 42, 1, 3, 1, 1);
  
}

//== Right Step================================================================================
void step_right () {
  servo[1].write(130);
  servo[3].write(50);
  servo[5].write(130);
  servo[7].write(50);
  delay(500);
  servo[2].write(130);
  servo[4].write(50);
  servo[6].write(40);
  servo[8].write(140);
  delay(300);
  servo[2].write(90);
  servo[4].write(90);
  servo[6].write(140);
  servo[8].write(40);
  delay(150);
  servo[2].write(130);
  servo[4].write(50);
  servo[6].write(40);
  servo[8].write(140);
  delay(700);
  
}
//== Left Step ================================================================================
void step_left () { 
  servo[1].write(130);
  servo[3].write(50);
  servo[5].write(130);
  servo[7].write(50);
  delay(500);
  servo[2].write(40);
  servo[4].write(140);
  servo[6].write(130);
  servo[8].write(50);
  delay(300);
  servo[2].write(140);
  servo[4].write(40);
  servo[6].write(90);
  servo[8].write(90);
  delay(150);
  servo[2].write(40);
  servo[4].write(140);
  servo[6].write(130);
  servo[8].write(50);
  delay(700);
  
}

//== Center Servos ======================================================================== 
void center_servos() {
  servo[1].write(90);
  servo[3].write(90);
  servo[5].write(90);
  servo[7].write(90);
  delay(500);
  servo[2].write(90);
  servo[4].write(90);
  servo[6].write(90);
  servo[8].write(90);
  delay(500);
  
}

//== minimal Servos ======================================================================== 
void minimal() {
  servo[2].write(140);
  servo[4].write(40);
  servo[6].write(140);
  servo[8].write(40);
  delay(300);
  servo[1].write(140);
  servo[3].write(40);
  servo[5].write(140);
  servo[7].write(40);
  delay(700);
  
}

//== Fighting Stance ======================================================================== 
void fightst() {
  servo[1].write(140);
  servo[3].write(90);
  servo[5].write(90);
  servo[7].write(40);
  servo[2].write(90);
  servo[4].write(90);
  servo[6].write(90);
  servo[8].write(90);
  delay(500);
  
}

//== lay ======================================================================== 
void lay() {
  servo[2].write(0);
  servo[4].write(180);
  servo[6].write(0);
  servo[8].write(180);
  delay(300);
  servo[1].write(140);
  servo[3].write(40);
  servo[5].write(140);
  servo[7].write(40);
  delay(700);
  
}

//== bow ======================================================================== 
void bow() {
  servo[1].write(140);
  servo[3].write(90);
  servo[5].write(90);
  servo[7].write(40);
  servo[2].write(90);
  servo[4].write(90);
  servo[6].write(90);
  servo[8].write(90);
  delay(300);
  servo[2].write(50);
  servo[8].write(130);
  delay(300);
  servo[2].write(90);
  servo[8].write(90);
  delay(300);
  servo[2].write(50);
  servo[8].write(130);
  delay(300);
  servo[2].write(90);
  servo[8].write(90);
  delay(700);
  
}

//== side to side ======================================================================== 
void side2side() {
  lean_left();
  lean_right();
  lean_left();
  lean_right();
  
}
//== Increase Speed ========================================================================
void increase_speed() {
  if (spd > 3) spd--;
}
//== Decrease Speed ======================================================================== 
void decrease_speed() {
  if (spd < 50) spd++;
}
//== Srv ===================================================================================
void srv( int p11, int p21, int p31, int p41, int p12, int p22, int p32, int p42, int sp1, int sp2, int sp3, int sp4) {
  // p11: Front Left Pivot Servo 
  // p21: Back Left Pivot Servo 
  // p31: Back Right Pivot Servo 
  // p41: Front Right Pivot Servo 
  // p12: Front Left Lift Servo 
  // p22: Back Left Lift Servo 
  // p32: Back Right Lift Servo 
  // p42: Front Right Lift Servo 
  // sp1: Speed 1 
  // sp2: Speed 2 
  // sp3: Speed 3 
  // sp4: Speed 4 
  // Multiply lift servo positions by manual height adjustment 
  p12 = p12 + high * 3;
  p22 = p22 + high * 3;
  p32 = p32 + high * 3;
  p42 = p42 + high * 3;
  
  while ((s11 != p11) || (s21 != p21) || (s31 != p31) || (s41 != p41) || (s12 != p12) || (s22 != p22) || (se32 != p32) || (s42 != p42)) {
    // Front Left Pivot Servo
    if (s11 < p11) // if servo position is less than programmed position 
    {
      if ((s11 + sp1) <= p11) s11 = s11 + sp1; // set servo position equal to servo position plus speed constant 
      else s11 = p11;
    }
    if (s11 > p11) // if servo position is greater than programmed position 
    {
      if ((s11 - sp1) >= p11) s11 = s11 - sp1; // set servo position equal to servo position minus speed constant 
      else s11 = p11;
    }
    // Back Left Pivot Servo 
    if (s21 < p21) {
      if ((s21 + sp2) <= p21) s21 = s21 + sp2 ;
      else s21 = p21;
    }
    if (s21 > p21) {
      if ((s21 - sp2) >= p21) s21 = s21 - sp2; 
      else s21 = p21;
    }
    // Back Right Pivot Servo 
    if (s31 < p31) {
      if ((s31 + sp3) <= p31) s31 = s31 + sp3;
      else s31 = p31;
    }
    if (s31 > p31) {
      if ((s31 - sp3) >= p31) s31 = s31 - sp3; 
      else s31 = p31;
    }
    // Front Right Pivot Servo 
    if (s41 < p41) {
      if ((s41 + sp4) <= p41) s41 = s41 + sp4;
      else s41 = p41;
    }
    if (s41 > p41) {
      if ((s41 - sp4) >= p41) s41 = s41 - sp4;
      else s41 = p41;
    }
    // Front Left Lift Servo 
    if (s12 < p12) {
      if ((s12 + sp1) <= p12) s12 = s12 + sp1;
      else s12 = p12;
    }
    if (s12 > p12) {
      if ((s12 - sp1) >= p12) s12 = s12 - sp1;
      else s12 = p12;
    }
    // Back Left Lift Servo 
    if (s22 < p22) { 
      if ((s22 + sp2) <= p22) s22 = s22 + sp2;
      else s22 = p22;
    }
    if (s22 > p22) {
      if ((s22 - sp2) >= p22) s22 = s22 - sp2;
      else s22 = p22;
    }
    // Back Right Lift Servo 
    if (se32 < p32) {
      if ((se32 + sp3) <= p32) se32 = se32 + sp3;
      else se32 = p32;
    }
    if (se32 > p32) {
      if ((se32 - sp3) >= p32) se32 = se32 - sp3;
      else se32 = p32;
    }
    // Front Right Lift Servo 
    if (s42 < p42) {
      if ((s42 + sp4) <= p42) s42 = s42 + sp4;
      else s42 = p42;
    }
    if (s42 > p42) {
      if ((s42 - sp4) >= p42) s42 = s42 - sp4;
      else s42 = p42;
    }
    // Write Pivot Servo Values 
    servo[1].write(s11 + da);
    servo[3].write(s21 + db);
    servo[5].write(s31 + dc);
    servo[7].write(s41 + dd);
    // Write Lift Servos Values 
    servo[2].write(s12);
    servo[4].write(s22);
    servo[6].write(se32);
    servo[8].write(s42);
    delay(spd);
    // Delay before next movement
  }// while
}//srv


