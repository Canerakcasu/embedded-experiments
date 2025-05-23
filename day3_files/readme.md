# ESP32 & Node-RED MQTT-Based Smart ARGB LED Control

This project aims to control an ARGB (NeoPixel) LED strip using an ESP32 development board. Control is achieved both through a Node-RED based web interface for setting individual LED colors and via a rotary encoder connected to the ESP32 for adjusting global brightness. Communication is handled using the MQTT protocol.

## 🌟 Features

* **Wi-Fi & MQTT Connection:** The ESP32 connects to the local network and an MQTT broker.
* **Node-RED Interface:** A web-based UI allows:

  * Selecting a specific LED.
  * Choosing a color for the selected LED.
  * Displaying the current brightness level.
* **Physical Control:** Using a rotary encoder:

  * Adjusting the global brightness of all LEDs (0-100%).
  * Toggling the LEDs ON/OFF with its push-button.
* **Individual Color & Global Brightness:** Retains individual LED colors set via Node-RED while adjusting their brightness globally with the encoder.
* **Status Feedback:** The ESP32 publishes its current brightness level back to Node-RED via MQTT, displayed on a gauge in the UI.

## 🛠️ Hardware Requirements

* ESP32 Development Board (Any model should work)
* ARGB LED Strip (WS2812B / NeoPixel - 15 LEDs assumed in the project)
* Rotary Encoder Module (with CLK, DT, SW, +, GND pins)
* Breadboard
* Jumper Wires
* (Recommended) 3 x 10K Ohm Resistors (for Rotary encoder pull-ups)
* Appropriate power supply for ESP32 (Micro USB cable)

## 💻 Software Requirements & Platforms

* **Arduino IDE:** For writing and uploading the ESP32 code.
* **MQTT Broker:** For communication between devices. (e.g., Mosquitto, HiveMQ Cloud)
* **Node-RED:** For creating the control UI and automation logic.
* **Node-RED Dashboard:** For providing UI elements for Node-RED. (`node-red-dashboard` package)
* **Arduino Libraries:**

  * `WiFi.h` (Comes with ESP32 Core)
  * `PubSubClient` (by Nick O'Leary)
  * `Adafruit_NeoPixel` (by Adafruit)
  * `ArduinoJson` (by Benoit Blanchon)
  * `ESP32Encoder` (by madhephaestus)

## 🔌 Hardware Setup & Wiring

**IMPORTANT:** Ensure the ESP32 is **not powered** while making connections!

**1. ARGB LED Strip Connection:**

| LED Pin | ESP32 Pin     | Description                  |
| :------ | :------------ | :--------------------------- |
| `GND`   | `GND`         | Ground                       |
| `VCC`   | `5V` / `3.3V` | Power (5V might be brighter) |
| `DIN`   | `GPIO 33`     | Data Input                   |

*Note: If using 5V for the LED and the ESP32 runs on 3.3V, using a **level shifter** for the `DIN` line (3.3V to 5V) is recommended for stable operation.*

**2. Rotary Encoder Connection:**

| Encoder Pin | ESP32 Pin | Description                                           |
| :---------- | :-------- | :---------------------------------------------------- |
| `+` (VCC)   | `3.3V`    | Power                                                 |
| `GND`       | `GND`     | Ground                                                |
| `CLK`       | `GPIO 32` | Clock Signal (Pull up to 3.3V with 10K recommended)   |
| `DT`        | `GPIO 35` | Data Signal (Pull up to 3.3V with 10K recommended)    |
| `SW`        | `GPIO 34` | Switch Signal (**Pull up to 3.3V with 10K required**) |

*Note: `GPIO 34` and `GPIO 35` do not support internal pull-up resistors. It is **highly recommended** to use external 10K Ohm pull-up resistors (connecting the pin to 3.3V) for these pins, especially `SW`.*

## ⚙️ Software Setup

**1. MQTT Broker:**

* Set up an MQTT broker on your local network (e.g., Mosquitto on a Raspberry Pi) or use a cloud service.
* Note down your broker's IP address and port.

**2. Node-RED:**

* Install Node-RED.
* Install `node-red-dashboard` from the "Manage palette" menu.

**3. Arduino IDE:**

* Install Arduino IDE and set up ESP32 board support.
* Install the required Arduino libraries (`PubSubClient`, `Adafruit_NeoPixel`, `ArduinoJson`, `ESP32Encoder`) using the Library Manager.

## 💾 ESP32 Code

The ESP32 code handles Wi-Fi/MQTT connections, reads the rotary encoder, processes MQTT commands, and controls the ARGB LED strip.

**Key Settings:** Before uploading, ensure you configure these variables in your `.ino` file:

* `ssid`: Your Wi-Fi network name.
* `password`: Your Wi-Fi password.
* `mqtt_server`: Your MQTT broker's IP address.
* Pin Definitions: Ensure `ARGB_LED_PIN`, `ENCODER_CLK_PIN`, `ENCODER_DT_PIN`, `ENCODER_SW_PIN` match your hardware connections.
* `ARGB_LED_COUNT`: The number of LEDs in your strip.

*(The full ESP32 code is not included in this README but should be available in your repository or from previous discussions.)*

## 📊 Node-RED Flow

Node-RED provides the user interface and sends color commands to the ESP32. You can import the JSON code into Node-RED (`Menu` > `Import`). **Configure the MQTT broker nodes** after importing.

*(Node-RED JSON flow code is provided above.)*

## 🚀 Usage

1. Upload the code to your ESP32 using the Arduino IDE.
2. Power on the ESP32 and check the Serial Monitor for connection statuses.
3. Deploy the Node-RED flow.
4. Open the Node-RED dashboard (e.g., http\://<node-red-ip>:1880/ui).
5. Select an LED index.
6. Pick a color and click "Send Color to Selected LED".
7. Rotate the encoder to adjust brightness globally.
8. Press the encoder to toggle all LEDs ON/OFF.
9. Monitor brightness on the Node-RED dashboard gauge.

## ⚠️ Troubleshooting & Notes

* **Encoder Instability:** Check external pull-ups on `SW`, `CLK`, and `DT`.
* **MQTT Issues:** Confirm broker IP and port match across ESP32 and Node-RED.
* **Wi-Fi Issues:** Double-check SSID and password.
* **Library Errors:** Verify all libraries are installed correctly.
