# MiniSkidi_4.0_BL_WIFI
Edited from https://github.com/Le0Michine/MiniSkidi-V4 and https://github.com/ProfBoots/MiniSkidi-V3.0

This is a combination of various parts of the above code to achieve what I wanted, certain things could possibly be done better as i'm no expert.

### Features

- Bluetooth controller support
- Web Interface with on screen joysticks, movement sequences and battery monitoring(based on two 16340 batteries in series)
- WiFi connection managed by https://github.com/tzapu/WiFiManager
- Neopixel LED strip support, I'm using a RGBW SK6812 strip on pin 1, which is the serial pin but it just happens to be what the 21 aux header goes to, I don't use serial much anyway so didn't mind
- The Arduino Sketch has [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA) to update it without plugging it in

Here are some example movement sequences, the first one is included by default as sequence 0.

<img src="seq1.gif" width="400"> <img src="seq2.gif" width="400">

### Web Interface Pictures

![img_wi1](wi1.png)

![img_wi2](wi2.png)
