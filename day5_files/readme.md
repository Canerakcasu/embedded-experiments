# ESP32 Advanced Control Hub

This project transforms an ESP32 microcontroller into a versatile control hub, featuring a web interface for managing an ARGB LED strip, a DFPlayer Mini MP3 module, and monitoring various inputs. Control is also possible via a directly connected rotary encoder.

---

## Features

### ✅ Web Interface:
- **Real-time status display** of all connected components.
- **Control ARGB LEDs:**
  - Toggle On/Off.
  - Adjust global brightness.
  - Select colors using an RGB color picker.
- **Control DFPlayer Mini:**
  - Play specific tracks by number.
  - Play/Resume, Pause, Next Track, Previous Track.
  - Adjust volume.
- **Display Status:**
  - Current WiFi Network (SSID).
  - Active mode of the rotary encoder.
  - Rotary encoder button press status (GPIO 14).
  - External button press status (GPIO 12).
  - SD Card status for DFPlayer.
  - Current LED color and brightness.
  - Current DFPlayer volume and playing track (number and name).

### 🔄 Rotary Encoder Control:
- Button press (on GPIO 14) cycles through control modes:
  1. LED Brightness Adjustment.
  2. LED Predefined Color Selection.
  3. DFPlayer Volume Adjustment.
- Rotation adjusts the value for the currently active mode.

### 🎵 DFPlayer Mini MP3 Module:
- Playback of MP3 files from an SD card.
- Files are expected in a `/mp3` folder, named `0001_filename.mp3`, `0002_anotherfile.mp3`, etc.
- Track names (configurable in the code) are displayed on the web UI.

### 💡 ARGB LED Strip (15 LEDs):
- Global color and brightness control.
- Data pin connected to GPIO 2.

### 📊 Status Monitoring:
- Real-time feedback via both Serial Monitor and the web interface.

---

## 🔌 Hardware Requirements

- ESP32 Development Board  
- DFPlayer Mini MP3 Module  
- Micro SD Card (FAT16 or FAT32 formatted, ≤ 32GB recommended)  
- Speaker (8 Ohm, ≤ 3W) for DFPlayer Mini  
- ARGB LED Strip (e.g., WS2812B, 15 LEDs in this configuration)  
- Rotary Encoder with Push Button (e.g., KY-040)  
- Push Button (for GPIO 12, if used as a separate input)  
- 1K Ohm Resistor (for DFPlayer RX line)  
- Jumper Wires  
- Breadboard (optional, for prototyping)  
- 5V Power Supply (recommended for DFPlayer and LED strip)  
- 3.3V Power Supply (ESP32 usually provides this for the rotary encoder)

---

## 🧭 Pin Connections

| Component          | ESP32 Pin        |
|-------------------|------------------|
| DFPlayer RX       | GPIO 17 (TX2) via 1K resistor |
| DFPlayer TX       | GPIO 16 (RX2)     |
| Rotary CLK        | GPIO 33           |
| Rotary DT         | GPIO 27           |
| Rotary SW         | GPIO 14           |
| External Button   | GPIO 12           |
| ARGB LED Strip    | GPIO 2 (Data In)  |

⚠️ **Important Note on Power:** Use a separate 5V power supply for the LED strip and DFPlayer. Connect **all GNDs together**.

---

## 🛠️ Software Setup

### 1. Install Arduino IDE
[Download from Arduino.cc](https://www.arduino.cc/)

### 2. Install ESP32 Board Support
- File → Preferences → Add URL:  
  `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- Tools → Board → Boards Manager → Search "esp32" and install.

### 3. Install Libraries
Open **Library Manager** and install:
- `Adafruit NeoPixel`
- `DFRobotDFPlayerMini`
- `AiEsp32RotaryEncoder`
- `ArduinoJson` (v6 recommended)

> `WiFi.h` and `WebServer.h` are included with ESP32 core.

### 4. Configure the Code
Edit the `.ino` file:

#### WiFi Credentials:
```cpp
const char* ssid = "Your_WiFi_SSID";
const char* password = "Your_WiFi_Password";
MP3 Track Names:



const char* trackNames[totalTracks + 1] = {
  "Unknown/Stopped",
  "Track 1 Name",
  "Track 2 Name",
  ...
};
Pin Definitions:

#define LED_PIN 2
#define ROTARY_ENCODER_A_PIN 33
#define ROTARY_ENCODER_B_PIN 27
#define ROTARY_ENCODER_BUTTON_PIN 14
#define DFPLAYER_RX_PIN 16
#define DFPLAYER_TX_PIN 17
#define EXTERNAL_BUTTON_D12_PIN 12

5. Prepare the SD Card
Format as FAT16/FAT32.

Create folder /mp3 and add files like:

/mp3/0001_trackname.mp3
/mp3/0002_othertrack.mp3
🚀 How to Use
Connect all hardware (double check power and resistors!).

Upload code from Arduino IDE to ESP32.

Open Serial Monitor at 115200 baud.

Note ESP32's IP address.

Open web browser → enter IP → control via UI.

Use the rotary encoder to:

Press to change mode

Rotate to adjust brightness / color preset / volume

🧩 Troubleshooting
DFPlayer Issues
Check 1K resistor on RX

Use correct wiring (ESP32 TX → DFPlayer RX)

Ensure SD card is formatted properly and named files correctly

Web Interface Not Loading
Ensure ESP32 is connected to WiFi

Your PC/Phone must be on the same network

Rotary Encoder Doesn't Work
Double-check CLK/DT/SW connections and pin numbers

LED Strip Doesn't Light Up
Check Data In and GND

Use external 5V supply for more than a few LEDs

Confirm correct LED_PIN and number of LEDs in code

🌱 Future Enhancements
IR remote support + IR learning via web UI

Advanced LED animations/effects

Segment control of LED strip via web

WiFi manager for AP-based setup

SPIFFS or LittleFS for web files

MQTT integration with Home Assistant

🤝 Contributing
Feel free to fork, improve, and submit pull requests. Issues and suggestions are welcome!

🖼️ Web Interface Preview
(day5_files/images/esp32_web_ui.png)