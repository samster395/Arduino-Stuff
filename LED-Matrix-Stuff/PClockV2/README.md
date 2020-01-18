# PClock - WIP
Note: The ways some of the functions are achieved may not be the best way to do it, I am not very experienced with Arduino. (If you can improve it, please let me know)

### Features
* NTP Time/Date
* Weather (Temperature, Condition, Humidity, Wind speed)
* Web interface with API (Port 82)
* [Phone Integration](#phone-integration) (Show when notification received or call incoming)
* Auto Brightness (Using phototransistor)
* [OTA Update](#ota-update)

### [Screenshots](https://imgur.com/a/QIhXM0w)

##### Libraries Used
* [PxMatrix](https://github.com/2dom/PxMatrix)
* [WiFiManager](https://github.com/tzapu/WiFiManager)
* [ArduinoJson](https://arduinojson.org/)

## Setup

See the PxMatrix Readme on how to wire the panel up and you might need to change some settings in the setup() to make it work correctly.

Edit the settings in the .ino file.

You will need an [OpenWeatherMap](https://openweathermap.org) API Key.

### First Boot

* Upon first boot the device will create an access point, connect to that and go to htttp://192.168.4.1 and connect it to the WiFi network.
* The device will then restart and flash the end of the IP on the screen, you can use that to go to the clock's web interface.

## Phone Integration

So the clock has some phone integration by being able to notify you when you get a notification or have a call incoming on your phone.

To use this you will need to use something like Tasker to send a GET request to the API (See the notification part of the API below) when a notification is received or a call is incoming, relatively simple to setup.

## API

### Change Mode

<code>/api?chmode=1</code>

Can also use <code>/api?mode=</code> and then the mode you want to change to

**Returns**: Mode Changed

### Update Weather

<code>/api?updw=1</code>

**Returns**: Weather Updated

### Restart

<code>/api?rst=1</code>

**Returns**: Restarting

### Status

Turn the display ON or OFF

<code>/api?status=</code>

**Returns**: Display turned ON/OFF

### Brightness

<code>/api?bright=</code>

**Returns**: Brightness changed to

### Version

<code>/api?vers=1</code>

Shows the software version number

### Notification

Shows noti received on the clock for 10 seconds

<code>/api?noti=other</code>

**Returns**: Received

### Notification - Call

Shows call incoming on the clock for 28 seconds - The time it takes a phone (well mine atleast) to go to voice mail

<code>/api?noti=call</code>

**Returns**: Received

### Rotation

An unused (by me) function to change between the mode (might not work properly)

<code>/api?rotation=1</code>

**Returns**: Rotation toggled


## OTA Update

The sketch can be updated over WiFi

1. Run a web server and put the espupdate folder on it and edit the index.php with your ESP's MAC Address
2. Set the URL in the settings
3. Export your compiled binaries from the Arduino IDE and put them on the web server in the bin folder with the name pclock-1.0.ino (the 1.0 being a version number higher than the current one on the ESP, Remember to change the version number in the sketch too, otherwise it will go into a update loop.)
