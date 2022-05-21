const bool DebugDisplayUpdate = true;
String apikey        = "";	// https://openweathermap.org/api
String NewsApiK      = "";	// https://newsapi.org/
const char server[]  = "api.openweathermap.org";
String City             = "";   
String Latitude         = "";              
String Longitude        = "";
String Language         = "EN";                        
String Hemisphere       = "north";                        
String Units            = "M";
String PowerSource      = "Battery"; // Set long term power source, decides whether to show batery level (Valid options: Battery, USB)                 
const char* Timezone    = "GMT0BST,M3.5.0/01,M10.5.0/02";  
const char* ntpServer   = "uk.pool.ntp.org";
int   gmtOffset_sec     = 0;
int   daylightOffset_sec = 3600;
bool debug = false;
