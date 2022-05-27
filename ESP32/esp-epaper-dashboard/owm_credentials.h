const bool DebugDisplayUpdate = true;
// WIFI is managed by https://github.com/tzapu/WiFiManager
// When your ESP starts up, it sets it up in Station mode and tries to connect to a previously saved Access Point
// if this is unsuccessful (or no previous network saved) it moves the ESP into Access Point mode and spins up a DNS and WebServer (default ip 192.168.4.1)
// using any wifi enabled device with a browser (computer, phone, tablet) connect to the newly created Access Point
String apikey        = "";	// https://openweathermap.org/api
String NewsApiK      = "";	// https://newsapi.org/
const char server[]  = "api.openweathermap.org";
String City             = "";   
String Latitude         = "";              
String Longitude        = "";
String Language         = "EN";                        
String Hemisphere       = "north";                        
String Units            = "M";
String PowerSource      = "Battery"; // Set long term power source, decides whether to show batery level and whether to refresh display at night (Valid options: Battery, USB)
int  WakeupHour      = 5;  // Wakeup after to save battery power
int  SleepHour       = 23; // Sleep  after to save battery power const char* Timezone    = "GMT0BST,M3.5.0/01,M10.5.0/02";  
const char* ntpServer   = "uk.pool.ntp.org";
int   gmtOffset_sec     = 0;
int   daylightOffset_sec = 3600;
bool debug = false;