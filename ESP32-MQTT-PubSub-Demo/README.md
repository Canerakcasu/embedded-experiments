# 📡 Simple IoT Project with ESP32, MQTT, and Node-RED

## 📝 Project Overview

This project showcases a simple yet effective **Internet of Things (IoT)** system using an **ESP32 microcontroller**, **MQTT protocol**, and the **Node-RED** visual programming tool.  
The aim is to establish two-way communication between the ESP32 and a software interface via MQTT, allowing for both data monitoring and remote control.

---

## 🛠️ Core Technologies

- **ESP32**: A low-cost microcontroller with built-in Wi-Fi and Bluetooth, ideal for IoT applications.
- **MQTT (Message Queuing Telemetry Transport)**: A lightweight, publish/subscribe messaging protocol optimized for low-bandwidth environments.
- **Node-RED**: A visual, flow-based development tool for wiring together devices, APIs, and online services.
- **Mosquitto**: An open-source MQTT broker used to manage MQTT message transmission.

---

## ⚙️ Project Functionality

- 📶 ESP32 connects to a local Wi-Fi network.
- 🔗 Connects to a Mosquitto MQTT broker (e.g., running on a Raspberry Pi or PC).
- 📤 Publishes data (e.g., sensor readings) to a predefined MQTT topic.
- 📥 Node-RED subscribes to that topic and receives the data.
- 🔁 Bidirectional communication: Node-RED can send control commands back to the ESP32 via a different topic.

---

## 🔩 Required Hardware

- ✅ ESP32 Development Board  
- ✅ Raspberry Pi (or any system to run the MQTT Broker)  
- ✅ Optional: Sensors (e.g., DHT11, PIR) and Actuators (e.g., LEDs, Relays)

---

## 💻 Required Software

- **Arduino IDE** – To program the ESP32  
- **Node-RED** – For flow logic and UI/dashboard  
- **Mosquitto** – MQTT Broker for message handling  

---

## 🚀 Setup Instructions

### 1. Install Mosquitto MQTT Broker

- Follow the official [Mosquitto installation guide](https://mosquitto.org/download/).
- Ensure the broker is up and accessible on your local network.

### 2. Configure the ESP32

- Open your sketch in **Arduino IDE**.
- Set your Wi-Fi credentials:
  ```cpp
  const char* ssid = "your-SSID";
  const char* password = "your-PASSWORD";

Set your MQTT broker IP:

const char* mqtt_server = "192.168.x.x"; // Replace with actual IP

Define MQTT topics for publishing/subscribing.

Upload the code to your ESP32.

3. Set Up Node-RED
Access Node-RED through your browser.

Import or create a new flow.

Configure MQTT nodes:

Set the broker address (same as ESP32).

Match MQTT topics with your ESP32 code.

Deploy the flow.

🔌 Basic Wiring (Optional)
While basic communication doesn't require additional wiring, you can enhance the project by adding:

Sensors (e.g., DHT11 for temperature/humidity, PIR for motion) to ESP32 input pins.

Actuators (e.g., LEDs, Relays, Motors) to ESP32 output pins for control via Node-RED.

▶️ How to Use
Power up the ESP32.

It will connect to Wi-Fi and the MQTT broker.

Begins publishing sensor/status data to the MQTT topic.

Node-RED receives and displays the data.

Use Node-RED UI (buttons, inject nodes, dashboard elements) to send commands back to the ESP32.

✨ Future Enhancements

✅ Error Handling: Add reconnect logic for Wi-Fi and MQTT failures.

✅ Sensor Integration: Expand with more sensors (e.g., light, gas, motion).

✅ Node-RED Dashboard: Build a responsive UI for data visualization and control.

✅ Actuator Control: Trigger devices like lights or motors remotely.

✅ Data Logging: Save sensor data to a database or CSV.

✅ Security: Use MQTT authentication and TLS encryption for secure communication.


🧠 Credits
Created using:

ESP32 by Espressif

Mosquitto MQTT Broker

Node-RED

Arduino IDE

👤 Contributors
Caner AKCASU