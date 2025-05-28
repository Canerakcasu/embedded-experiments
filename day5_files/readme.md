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

# IR Remote Control Integration – ESP32 Project

This ESP32 project supports extensive control via a standard infrared (IR) remote control. IR control provides fast access to common functions, eliminating the need for the web interface or Node-RED in many scenarios.

(day5_files/images/node_red_dashboard_remote_control.png)

## 📦 Hardware Setup

### 🔌 IR Receiver Module
Use a standard 3-pin TSOP-type IR receiver (e.g., TSOP1738, TSOP4838, VS1838B):

- **VCC** → ESP32 **3.3V**
- **GND** → ESP32 **GND**
- **Data/Out** → ESP32 **GPIO 18** (defined as `IR_RECEIVE_PIN` in the code)

(day5_files/images/upd_remote_control.png)

## ⚙️ Functionality Overview

The ESP32 processes specific HEX codes from an IR remote to control:

- **LED Power**: Toggle ARGB LEDs on/off
- **DFPlayer Mini**:
  - Play, pause, stop, next/previous track
  - Fast forward, rewind
  - Volume up/down, mute toggle
- **Mode Switching**: Use the "Enter" button to switch the D-Pad mode

---

## 🎮 D-Pad Modes (`irDpadMode`)

Pressing the **Enter** button cycles through the following modes:

### 🔹 Mode 0 – LED Control
- **Up** → Increase LED brightness  
- **Down** → Decrease LED brightness  
- **Left** → Previous LED color  
- **Right** → Next LED color  

### 🔹 Mode 1 – DFPlayer Control
- **Up** → Volume up  
- **Down** → Volume down  
- **Left** → Previous track  
- **Right** → Next track  

The current D-Pad mode (`irDpadMode`) is visible in the web interface and published via MQTT.

---

## 🔢 IR HEX Code Mapping

These HEX codes correspond to a specific remote used during development. If you use a different remote, see the next section to reconfigure.

| Remote Button      | Action                         | HEX Code     |
|--------------------|--------------------------------|--------------|
| Mute               | DFPlayer Mute Toggle           | `0xED127F80` |
| Power              | Toggle LEDs On/Off             | `0xE11E7F80` |
| Music              | Play Track 1                   | `0xFD027F80` |
| Play/Stop          | Play/Pause Toggle              | `0xFB047F80` |
| Up Arrow           | D-Pad Up (depends on mode)     | `0xFA057F80` |
| Down Arrow         | D-Pad Down (depends on mode)   | `0xE41B7F80` |
| Left Arrow         | D-Pad Left (depends on mode)   | `0xF8077F80` |
| Right Arrow        | D-Pad Right (depends on mode)  | `0xF6097F80` |
| Enter              | Toggle D-Pad Mode              | `0xF7087F80` |
| Volume Up          | DFPlayer Volume Up             | `0xF30C7F80` |
| Volume Down        | DFPlayer Volume Down           | `0xFF007F80` |
| FFWD              | DFPlayer Next Track            | `0xF00F7F80` |
| REW               | DFPlayer Previous Track        | `0xF20D7F80` |
| Next              | DFPlayer Next Track            | `0xF10E7F80` |
| Back              | DFPlayer Previous Track        | `0xE6197F80` |

---
(day5_files/images/node-red_flow_remotecontrol.png)
## 🌐 Web UI & Node-RED Simulation

You can simulate IR remote control actions from both the web interface and Node-RED.


### ESP32 Web Interface
- Endpoint: `/ir_action?cmd=<action_name>`
- Example: `/ir_action?cmd=power`

### Node-RED Dashboard
- Publishes to MQTT topic: `esp32/command/ir_action`
- Payload: `<action_name>` (e.g., `mute`, `play_stop`, `power`)

The ESP32 maps these strings using `getHexForIrAction()` and processes them via `processIRCode()`.

---

## 🛠️ Using a Different IR Remote

The listed HEX codes are remote-specific. To use another remote:

1. **Read HEX Codes from Your Remote**  
   - Use a simple IR reader sketch to print `IrReceiver.decodedIRData.decodedRawData` to the Serial Monitor.
2. **Update Code**
   - Modify `processIRCode(uint32_t irCode)` to match your new HEX values.
   - Update `getHexForIrAction(String actionName)` to reflect these changes.
3. **Upload the updated sketch** to your ESP32.

---

## 💡 Future Enhancement (Planned)

> **Web-based IR Learning Mode**  
> A feature allowing users to assign new IR codes to actions from the web UI, saving the mappings to non-volatile storage (NVS), eliminating the need to edit and re-upload code.

---

### 📁 Related Files

- `main.cpp` – IR decoding and action processing logic  
- `ir_remote.h` – Action name to HEX code mapping helper  
- `web_server.cpp` – Handles `/ir_action` endpoint  
- `mqtt_handler.cpp` – Handles incoming MQTT IR action commands  

---

### ✅ License

This project is open-source and available under the MIT License.
