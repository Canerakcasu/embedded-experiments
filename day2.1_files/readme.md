# 🎨 Node-RED + ESP32 ARGB LED Control

A project to individually control a 15-LED ARGB (WS2812B or similar) strip using an ESP32 microcontroller and a Node-RED dashboard UI. Communication is handled via MQTT over Wi-Fi.

## ✨ Features

- 🎯 Individual color control for each LED on a 15-LED strip.
- 🖥️ Node-RED Dashboard interface:
  - Dropdown menu for LED selection.
  - Visual color picker (via `iro.js`).
  - Button to send selected color to the ESP32.
- 📡 MQTT-based communication between Node-RED and ESP32.
- 🔧 Easily expandable and customizable.

---

## 🧰 Requirements

### 🔌 Hardware

- ESP32 Dev Board (e.g., ESP32-WROOM-32)
- ARGB LED Strip (WS2812B/SK6812, 15 LEDs assumed)
- 5V Power Supply (rated for your LED count)
- Jumper wires
- Optional:
  - 300–500Ω resistor between ESP32 data pin and LED DIN
  - 1000µF capacitor across VCC and GND at LED input

### 💻 Software

#### ESP32 (Arduino IDE or PlatformIO)

- `Adafruit_NeoPixel`
- `WiFi` (ESP32 Core)
- `PubSubClient`
- `ArduinoJson`

#### Node-RED

- `node-red-dashboard`
- `node-red-contrib-ui-iro-color-picker`
- Core nodes: `mqtt in/out`, `function`, `change`, `ui_dropdown`, `ui_button`

#### MQTT Broker

- Mosquitto or any MQTT broker (e.g., on Raspberry Pi)

---

## ⚙️ Setup and Configuration

### 1. ESP32 Configuration

1. Install required libraries in Arduino IDE.
2. Update the following variables in the `.ino` file:
   ```cpp
   const char* ssid = "YOUR_SSID";
   const char* password = "YOUR_PASSWORD";
   const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
   #define ARGB_LED_PIN 33
   #define ARGB_LED_COUNT 15

Upload the code to the ESP32.

Monitor via Serial Monitor at 115200 baud for connection status.

Wiring:
ESP32 GPIO33 (via 330Ω) --> LED DIN  
ESP32 GND --> LED GND --> Power Supply GND  
5V Power Supply --> LED VCC

2. MQTT Broker
Install Mosquitto on your server (e.g., Raspberry Pi).

Enable anonymous access or configure credentials.

Ensure MQTT port (default 1883) is open and accessible.

3. Node-RED Flow
Install required nodes via Palette Manager.

Create a new flow with:

ui_dropdown:

Label: "Select LED (1–15)"

Options: Numbers 1–15, values 0–14

iro-color-picker:

Outputs { r, g, b } object

ui_button:

Label: "Set LED Color"

Flow Logic:

ui_dropdown → change node → flow.selectedLedIndex

iro-color-picker → change node → flow.selectedColorObject

ui_button → function node:

js
Copy
Edit
let index = flow.get("selectedLedIndex");
let color = flow.get("selectedColorObject");

let hex = "#" + [color.r, color.g, color.b].map(x => {
    return ("0" + x.toString(16)).slice(-2);
}).join("");

msg.payload = {
  index: index,
  color: hex
};

return msg;
function → mqtt out node:

MQTT Topic: esp32/setSingleLed

Server: Your MQTT broker details

Deploy the flow.

🚀 Usage
Power the ESP32 and LED strip.

Open Node-RED Dashboard (http://<your-ip>:1880/ui).

Select LED index and color.

Click "Set LED Color".

Watch the selected LED light up with your chosen color 🎉

📡 MQTT Format
Topic:
esp32/setSingleLed

Payload Example:

json
Copy
Edit
{
  "index": 0,
  "color": "#FF0000"
}
🛠️ Troubleshooting
Problem	Solution
ESP32 not connecting	Check Wi-Fi credentials, MQTT server IP, monitor Serial output
No MQTT messages received	Verify broker is running, test using MQTT tools like MQTT Explorer
LED not lighting up	Check wiring (especially GND and DIN), ensure sufficient power supply
Wrong color or no effect	Debug Node-RED output payload, confirm topic and format are correct

📌 Notes
Power your LED strip separately if using more than a few LEDs.

Use a logic-level shifter if LED strip requires 5V logic and your ESP32 outputs 3.3V (usually works without one for WS2812B).

Expand the Node-RED UI to support saving presets, animations, or full-strip patterns.

