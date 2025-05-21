# 📡 Simple IoT Project with ESP32, MQTT, and Node-RED

## 📝 Project Overview
This project demonstrates a basic **Internet of Things (IoT)** setup using an **ESP32 microcontroller**, **MQTT messaging**, and the **Node-RED** visual programming tool.  
The primary goal is to establish robust communication between an ESP32 and a software platform (Node-RED) via MQTT.

---

## 🛠️ Core Technologies

- **ESP32**: A low-cost, low-power system-on-a-chip (SoC) with integrated Wi-Fi and Bluetooth capabilities. Used to send and receive data.
- **MQTT**: A lightweight, publish-subscribe network protocol ideal for low-bandwidth environments.
- **Node-RED**: A flow-based development tool for visual programming to process and visualize data.
- **Mosquitto**: An open-source MQTT broker that relays messages between publishers and subscribers.

---

## ⚙️ Functionality

1. ESP32 connects to a local Wi-Fi network.
2. Establishes MQTT connection with a Mosquitto broker (e.g., running on a Raspberry Pi).
3. Publishes data (e.g., sensor values) to a specific MQTT topic.
4. Node-RED subscribes to this topic to receive and process/display data.
5. Node-RED can also publish commands to another topic that ESP32 listens to, enabling bidirectional communication.

---

## 🔩 Hardware

- ✅ ESP32 Development Board  
- ✅ Raspberry Pi (or any computer to run the MQTT Broker)

---

## 💻 Software

- **Arduino IDE** – For programming the ESP32  
- **Node-RED** – For building logic and dashboards  
- **Mosquitto MQTT Broker** – Message queuing system  

---

## 🚀 Setup Instructions

### 1. Install Mosquitto on Raspberry Pi (or your Broker Host)

- Follow the official [Mosquitto installation guide](https://mosquitto.org/download/).
- Make sure the broker is accessible within your network.

### 2. ESP32 Code Configuration

- Open the sketch in **Arduino IDE**.
- Set your Wi-Fi credentials:
  ```cpp
  const char* ssid = "your-SSID";
  const char* password = "your-PASSWORD";

Set MQTT broker IP address:

const char* mqtt_server = "192.168.x.x"; // Replace with broker IP

Define MQTT topics for publishing and subscribing.

Upload the code to your ESP32 board.

3. Node-RED Flow Setup
Open Node-RED in your browser.

Import or create a new flow.

Configure MQTT input/output nodes:

Set Server to the IP of the MQTT broker.

Match MQTT Topics used in ESP32 code.

Deploy your flow.

🔌 Wiring
Basic MQTT communication requires no wiring.

However, for sensor/actuator integration:

Input: Connect sensors (e.g., DHT11, PIR) to ESP32 input pins.

Output: Connect actuators (e.g., LED, relay) to ESP32 output pins.

▶️ Usage
Power on ESP32 – it connects to Wi-Fi and MQTT broker.

Starts publishing data to the defined topic.

Node-RED receives and displays/processes the data.

Use Node-RED UI (buttons, inject nodes) to send control messages to ESP32.

✨ Future Improvements
✅ Add Wi-Fi/MQTT error handling

✅ Integrate multiple sensors for real-time monitoring

✅ Build a dashboard in Node-RED

✅ Enable remote control of actuators

✅ Implement data logging (e.g., to a database)

✅ Add security features like MQTT auth and TLS