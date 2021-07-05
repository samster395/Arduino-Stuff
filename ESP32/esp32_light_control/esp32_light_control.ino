#include <WiFi.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>

Servo myservo;  // create servo object to control a servo
Servo myservo2;
// twelve servo objects can be created on most boards

// GPIO the servo is attached to
static const int servoPin = 13;
static const int servoPin2 = 15;
#define LedPIN            5
int HmPitch = 71;
int HmYaw = 102;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LedPIN, NEO_GRB + NEO_KHZ800);

// Replace with your network credentials
const char* ssid     = "";
const char* password = "";

// Set web server port number to 80
WiFiServer server(80);

void HmLight() {
myservo.write(HmPitch);
myservo2.write(HmYaw);
pixels.setBrightness(255);
pixels.setPixelColor(0, pixels.Color(0,0,255)); // Green
pixels.show(); // This sends the updated pixel color to the hardware. 
}

// Variable to store the HTTP request
String header;

// Decode HTTP GET value
String valueStringH = String(5);
int posH1 = 0;
int posH2 = 0;
String valueStringA = String(5);
int posA1 = 0;
int posA2 = 0;
String valueStringL = String(5);
int posL1 = 0;
int posL2 = 0;
byte red = 0;
byte green = 0;
byte blue = 0;

const char Head[] PROGMEM = R"=====(
             <!DOCTYPE html><html>
            <head><meta name="viewport" content="width=device-width, initial-scale=1">
            <style>
body {
    background: #2c3e50ff;
    color: red;
}
</style>
<title>Light Control</title>
            <style>body { text-align: center; font-family: "Trebuchet MS", Arial; margin-left:auto; margin-right:auto;}
            .slider { width: 300px; } .slider2 { width: 300px; }</style>
            <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>
)=====";

const char Foot[] PROGMEM = R"=====(
            <script>
            function hexToRgb(hex) {
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
    } : null;
}

            $.ajaxSetup({timeout:1000}); function led() { 
            var x = document.getElementById("myColor").value;
            $.get("?led=R" + hexToRgb(x).r  + "G" + hexToRgb(x).g  + "B" + hexToRgb(x).b + "&"); 
            {Connection: close};
            }
                        
      var slider = document.getElementById("servoSlider");
            var servoP = document.getElementById("servoPos"); 
      servoP.innerHTML = slider.value;
            slider.oninput = function() { slider.value = this.value; servoP.innerHTML = this.value; }
            $.ajaxSetup({timeout:1000}); function servo(pos) { 
            $.get("?pitch=" + pos + "&"); 
            {Connection: close};
            }
      
      var slider2 = document.getElementById("servoSlider2");
            var servoP2 = document.getElementById("servoPos2"); 
            servoP2.innerHTML = slider2.value;
            slider2.oninput = function() { slider2.value = this.value; servoP2.innerHTML = this.value; }
            $.ajaxSetup({timeout:1000}); function servo2(pos) { 
            $.get("?yaw=" + pos + "&"); 
            {Connection: close};
            }

            $.ajaxSetup({timeout:1000}); function Home() { 
            $.get("?home=" + 1 + "&"); 
            {Connection: close};
            }

             $.ajaxSetup({timeout:1000}); function LEDo() { 
            $.get("?off=" + 1 + "&"); 
            {Connection: close};
            }
            </script>
            </body></html>
)=====";

void setup() {
  Serial.begin(9600);

  myservo.attach(servoPin);  // attaches the servo on the servoPin to the servo object
  myservo2.attach(servoPin2);
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  pixels.begin(); // This initializes the NeoPixel library.
  HmLight();
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.write(Head);
            
            client.println("</head><body><h1>Light Control</h1>");
            
            client.println("<button onclick=\"Home()\">Home Light</button> | <button onclick=\"LEDo()\">Turn LED off</button><br><br>");
              
            client.println("Select LED colour: <input type=\"color\" id=\"myColor\" value=\"#0000ff\"> <button onclick=\"led()\">Send</button>");   
            
            client.println("<p>Pitch: <span id=\"servoPos\"></span></p>");          
            client.println("<input type=\"range\" min=\"0\" max=\"180\" class=\"slider\" id=\"servoSlider\" onchange=\"servo(this.value)\" value=\""+valueStringH+"\"/>");

            client.println("<p>Yaw: <span id=\"servoPos2\"></span></p>");          

            client.println("<input type=\"range\" min=\"0\" max=\"180\" class=\"slider2\" id=\"servoSlider2\" onchange=\"servo2(this.value)\" value=\""+valueStringA+"\"/>");

            client.write(Foot);

            //GET /?value=180& HTTP/1.1
            if(header.indexOf("GET /?pitch=")>=0) {
              posH1 = header.indexOf('=');
              posH2 = header.indexOf('&');
              valueStringH = header.substring(posH1+1, posH2);
              
              //Rotate the servo
              myservo.write(valueStringH.toInt());
              Serial.println(valueStringH); 
            }     
            //GET /?value2=180& HTTP/1.1
            if(header.indexOf("GET /?yaw=")>=0) {
              posA1 = header.indexOf('=');
              posA2 = header.indexOf('&');
              valueStringA = header.substring(posA1+1, posA2);
              
              //Rotate the servo
              myservo2.write(valueStringA.toInt());
              Serial.println(valueStringA); 
            }        

           if(header.indexOf("GET /?led=")>=0) {
              posL1 = header.indexOf('=');
              posL2 = header.indexOf('&');
              valueStringL = header.substring(posL1 + 1, posL2);

              red = valueStringL.substring(valueStringL.indexOf('R') + 1,valueStringL.indexOf('G')).toInt();
              green = valueStringL.substring(valueStringL.indexOf('G') + 1,valueStringL.indexOf('B')).toInt();
              blue = valueStringL.substring(valueStringL.indexOf('B') + 1,valueStringL.indexOf('&')).toInt();

              pixels.setBrightness(255);
              pixels.setPixelColor(0, pixels.Color(red,green,blue));
              pixels.show(); // This sends the updated pixel color to the hardware.
              
              Serial.println(valueStringL); 
            } 

            if(header.indexOf("GET /?home=")>=0) {
              HmLight();
              Serial.println("Light Homed"); 
            }     
            
            if(header.indexOf("GET /?off=")>=0) {
              HmLight();
              pixels.setBrightness(0);
              pixels.show();
              Serial.println("Light Turned off"); 
            }     
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
  
}
