#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Servo.h>
#include <neotimer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // 
#define MAX_SERVO_NUM 2
Servo servo[MAX_SERVO_NUM+1];
WiFiServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

Neotimer BTYtimer = Neotimer(5000); // Set timer's preset to 5s
Neotimer BMEtimer = Neotimer(10000); // Set timer's preset to 10s
 
char ssid[] = "Action Cam-3887";
char pass[]= "1234567890";

bool battery = false;

String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

String html_2 = R"=====(
<!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<script> <!-- Websocket recconect library -->
!function(a,b){"function"==typeof define&&define.amd?define([],b):"undefined"!=typeof module&&module.exports?module.exports=b():a.ReconnectingWebSocket=b()}(this,function(){function a(b,c,d){function l(a,b){var c=document.createEvent("CustomEvent");return c.initCustomEvent(a,!1,!1,b),c}var e={debug:!1,automaticOpen:!0,reconnectInterval:1e3,maxReconnectInterval:3e4,reconnectDecay:1.5,timeoutInterval:2e3};d||(d={});for(var f in e)this[f]="undefined"!=typeof d[f]?d[f]:e[f];this.url=b,this.reconnectAttempts=0,this.readyState=WebSocket.CONNECTING,this.protocol=null;var h,g=this,i=!1,j=!1,k=document.createElement("div");k.addEventListener("open",function(a){g.onopen(a)}),k.addEventListener("close",function(a){g.onclose(a)}),k.addEventListener("connecting",function(a){g.onconnecting(a)}),k.addEventListener("message",function(a){g.onmessage(a)}),k.addEventListener("error",function(a){g.onerror(a)}),this.addEventListener=k.addEventListener.bind(k),this.removeEventListener=k.removeEventListener.bind(k),this.dispatchEvent=k.dispatchEvent.bind(k),this.open=function(b){h=new WebSocket(g.url,c||[]),b||k.dispatchEvent(l("connecting")),(g.debug||a.debugAll)&&console.debug("ReconnectingWebSocket","attempt-connect",g.url);var d=h,e=setTimeout(function(){(g.debug||a.debugAll)&&console.debug("ReconnectingWebSocket","connection-timeout",g.url),j=!0,d.close(),j=!1},g.timeoutInterval);h.onopen=function(){clearTimeout(e),(g.debug||a.debugAll)&&console.debug("ReconnectingWebSocket","onopen",g.url),g.protocol=h.protocol,g.readyState=WebSocket.OPEN,g.reconnectAttempts=0;var d=l("open");d.isReconnect=b,b=!1,k.dispatchEvent(d)},h.onclose=function(c){if(clearTimeout(e),h=null,i)g.readyState=WebSocket.CLOSED,k.dispatchEvent(l("close"));else{g.readyState=WebSocket.CONNECTING;var d=l("connecting");d.code=c.code,d.reason=c.reason,d.wasClean=c.wasClean,k.dispatchEvent(d),b||j||((g.debug||a.debugAll)&&console.debug("ReconnectingWebSocket","onclose",g.url),k.dispatchEvent(l("close")));var e=g.reconnectInterval*Math.pow(g.reconnectDecay,g.reconnectAttempts);setTimeout(function(){g.reconnectAttempts++,g.open(!0)},e>g.maxReconnectInterval?g.maxReconnectInterval:e)}},h.onmessage=function(b){(g.debug||a.debugAll)&&console.debug("ReconnectingWebSocket","onmessage",g.url,b.data);var c=l("message");c.data=b.data,k.dispatchEvent(c)},h.onerror=function(b){(g.debug||a.debugAll)&&console.debug("ReconnectingWebSocket","onerror",g.url,b),k.dispatchEvent(l("error"))}},1==this.automaticOpen&&this.open(!1),this.send=function(b){if(h)return(g.debug||a.debugAll)&&console.debug("ReconnectingWebSocket","send",g.url,b),h.send(b);throw"INVALID_STATE_ERR : Pausing to reconnect websocket"},this.close=function(a,b){"undefined"==typeof a&&(a=1e3),i=!0,h&&h.close(a,b)},this.refresh=function(){h&&h.close()}}return a.prototype.onopen=function(){},a.prototype.onclose=function(){},a.prototype.onconnecting=function(){},a.prototype.onmessage=function(){},a.prototype.onerror=function(){},a.debugAll=!1,a.CONNECTING=WebSocket.CONNECTING,a.OPEN=WebSocket.OPEN,a.CLOSING=WebSocket.CLOSING,a.CLOSED=WebSocket.CLOSED,a});
</script>
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
.slider {
  -webkit-appearance: none;
  width: 100%;
  height: 25px;
  background: #d3d3d3;
  outline: none;
  opacity: 0.7;
  -webkit-transition: .2s;
  transition: opacity .2s;
}

.slider:hover {
  opacity: 1;
}

.slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 25px;
  height: 25px;
  background: #ff0000;
  cursor: pointer;
}

.slider::-moz-range-thumb {
  width: 25px;
  height: 25px;
  background: #4CAF50;
  cursor: pointer;
}
.small {
  font-size: 30px;
}

.big {
  font-size: 52px;
}
</style>
<title>Gimbal Control</title>
<body>
</head><body>
<script>
  var Socket;
  function init() 
  {
    Socket = new ReconnectingWebSocket('ws://' + window.location.hostname + ':81/');
    Socket.onmessage = function(event) { processReceivedCommand(event); };
  }

function processReceivedCommand(evt) 
{
    if (evt.data.includes("battery") === true) {
      document.getElementById("bts").innerHTML = String(evt.data); 
    } else if (evt.data.includes("Temperature") === true) {
      document.getElementById("temp").innerHTML = String(evt.data); 
    }
    //console.log(evt.data);
}
  
function WSt(){
  sendText("hello1");
}

function toggleElement(divID) {
  var x = document.getElementById(divID);
  if (x.style.display === "none") {
    x.style.display = "block";
  } else {
    x.style.display = "none";
  }
}

  function sendText(data)
  {
    Socket.send(data);
  }
 
  window.onload = function(e)
  { 
    init();
  }
</script>
<!--<h1>Gimbal Control</h1>-->
<p id="bts"></p>
<div id="serC" style="font-size: 50px;">

Rotation:<br>
<button onclick="sendText('s1rlc');" class="small">&#8249; Cont</button>
<button onclick="sendText('s1rl');" class="big">&#8249;</button>
<button onclick="sendText('s1rr');" class="big">&#8250;</button>
<button onclick="sendText('s1rrc');" class="small">&#8250; Cont</button>
<button style="margin: 10px;" onclick="sendText('s1s');" class="big">Stop</button>
<br>

<form>
<table cellpadding="0" cellspacing="0" border="0" style="margin-left: auto; margin-right:auto;">
<tbody><tr>
<td rowspan="2">Tilt: <input type="number" name="servoPos2" id="servoPos2" onchange="s2pos.a = this.value;" value="90" min="0" max="180" style="width: 100px;height: 94px;font-weight:bold;font-size: 50px;"></td>
<td><input type="button" value=" /\ " onclick="s2pos.a++;" style="font-size: 30px;margin:0;padding:0;width: 40px;height: 50px;"></td>
</tr>
<tr>
<td><input type="button" value=" \/ " onclick="s2pos.a--;" style="font-size: 30px;margin:0;padding:0;width: 40px;height: 50px;"></td>
</tr>
</tbody></table>
</form>
<button onclick="s2pos.a = 35;" style="height: 50px;font-size: 25px;font-weight: bold;">Straight down</button> <button onclick="home()" style="width: 100px;height: 50px;font-size: 25px;font-weight: bold;">Home</button> <button onclick="s2pos.a = 130;" style="height: 50px;font-size: 25px;font-weight: bold;">Straight forward</button><br>
<input type="range" class="slider" min="0" max="180" id="s2" onchange="s2pos.a = this.value;" value="90" step="1" style="width:80%;">
<br>
<!--<button onclick="WSt();" style="height: 50px;font-size: 25px;font-weight: bold;">WS Test</button>-->
</div>
<div id="temp"></div>
<script>
s2pos = {
  aInternal: 10,
  aListener: function(val) {},
  set a(val) {
    this.aInternal = val;
    this.aListener(val);
  },
  get a() {
    return this.aInternal;
  },
  registerListener: function(listener) {
    this.aListener = listener;
  }
}
s2pos.a = 90;
s2pos.registerListener(function(val) {
  sendText('s,2,'+ val);
  document.getElementById("s2").value = val;
  document.getElementById("servoPos2").value = val;
  //console.log("Someone changed the value of x2.a to " + val);
});

function home(){
  s2pos.a = 90;
}
</script>
</div>

</body>
</html>
)=====";

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Serial started at 115200");
  Serial.println();
 
  // Connect to a WiFi network
  Serial.print(F("Connecting to "));  Serial.println("ssid");
  WiFi.begin(ssid,pass);

  // connection with timeout
  int count = 0; 
  while ( (WiFi.status() != WL_CONNECTED) && count < 17) 
  {
      Serial.print(".");  delay(500);  count++;
  }
 
  if (WiFi.status() != WL_CONNECTED)
  { 
     Serial.println("");  Serial.print("Failed to connect to ");  Serial.println(ssid);
     while(1);
  }
 
  Serial.println("");
  Serial.println(F("[CONNECTED]"));   Serial.print("[IP ");  Serial.print(WiFi.localIP()); 
  Serial.println("]");

  attachServo();
 
  // start a server
  server.begin();
  Serial.println("Server started");
 
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

    unsigned status;
    // default settings
    // (you can also pass in a Wire library object like &Wire2)
    status = bme.begin();  
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
        Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
        Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
        Serial.print("        ID of 0x60 represents a BME 280.\n");
        Serial.print("        ID of 0x61 represents a BME 680.\n");
        while (1);
    }
}

void loop()
{

  if (battery){
  if(BTYtimer.repeat()){
    String bts = "Battery status = ";
    bts = bts + random(0,100);
    webSocket.broadcastTXT(bts + "%");  
  }
  }

    if(BMEtimer.repeat()){
    printValues();
  }
    webSocket.loop();
 
    WiFiClient client = server.available();     // Check if a client has connected
    if (!client)  {  return;  }
 
    client.flush();
    client.print( header );
    client.print( html_2 ); 
    Serial.println("New page served");

    delay(5);
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length)
{
  if(type == WStype_TEXT)
  { 
    String pl;
    for(int i = 0; i < length; i++) { pl += (char) payload[i]; }
    
      if (pl == "s1rl") {
          s1RL();        
      } else if (pl == "s1rr") {
          s1RR();        
      } else if (pl == "s1rlc") {
          s1RLc();        
      } else if (pl == "s1rrc") {
          s1RRc();        
      } else if (pl == "s1s") {
          s1S();        
      } else if (pl == "up") {
          Serial.println(millis());
          webSocket.broadcastTXT("shutup");    
      } else {
          Serial.print("WS payload = ");
          Serial.println(pl); 
      }

  String parseArg = getValue(pl, ',', 0); // if  a,4,D,r  would return D 
   //Serial.println(parseArg); 
if (parseArg == "s" || parseArg == "S"){
  int servoN = getValue(pl, ',', 1).toInt(); // if  a,4,D,r  would return D 
  int sAngle = getValue(pl, ',', 2).toInt(); // if  a,4,D,r  would return D
  if ( 0 <= servoN &&  servoN < MAX_SERVO_NUM+1 )
      {
        // Single Servo Move
        servo[servoN].write(sAngle);

        // Wait Servo Move
        //delay(300); // 180/60(Ã‚Â°/100msecÃ¯Â¼â€°=300(msec)
      }
  //Debug.println("Servo moved");
}
      
  }
 
  else 
  {
    Serial.print("WStype = ");   Serial.println(type);  
    Serial.print("WS payload = ");
    for(int i = 0; i < length; i++) { Serial.print((char) payload[i]); }
    Serial.println();
 
  }
}

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

void attachServo()
{
  servo[1].attach(13);
  servo[2].attach(12);
}

void detachServo()
{
  servo[1].detach();
  servo[2].detach();
}

//== Center Servos ======================================================================== 
void center_servos() {
  servo[1].write(90);
  servo[2].write(90);
}

void s1RL() {
  servo[1].write(83);
  delay(500);
  servo[1].write(90);
}

void s1RR() {
  servo[1].write(100);
  delay(500);
  servo[1].write(90);
}

void s1RLc() {
  servo[1].write(85);
}

void s1RRc() {
  servo[1].write(97);
}

void s1S() {
  servo[1].write(90);
}

void printValues() {
    String temp = "Temperature = ";
    temp += bme.readTemperature();
    temp += " C";
    temp += "<br>";

    temp += "Humidity = ";
    temp += bme.readHumidity();
    temp += " %";
    temp += "<br>";
    
    temp += "Pressure = ";
    temp += bme.readPressure() / 100.0F;
    temp += " hPa";
    temp += "<br>";

    temp += "Approx. Altitude = ";
    temp += bme.readAltitude(SEALEVELPRESSURE_HPA);
    temp += " m";
    
    webSocket.broadcastTXT(temp); 
    
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" *C");

    Serial.print("Pressure = ");
    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");

    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");

    Serial.println();
}
