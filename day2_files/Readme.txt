MQTT Topics
nodered/komut/gelen → ESP32 subscribes (receives messages)

esp32/veri/giden → ESP32 publishes (sends messages)

🛠 Setup Instructions
1️⃣ MQTT Broker
Ensure your MQTT Broker is up and running at 192.168.20.208:1883. If using a different address, update:

mqtt_server_ip in the ESP32 code

MQTT config in Node-RED nodes

2️⃣ ESP32 Setup
Upload the provided ESP32_MQTT_Manual_Communication.ino to the ESP32 using Arduino IDE

Configure your Wi-Fi credentials and MQTT broker IP

Open the Serial Monitor (baud: 115200) to send/receive messages

3️⃣ Node-RED Setup
Import the provided flow (e.g., node_red_flow.json)

Install node-red-dashboard if not already installed

Dashboard Tabs:

Send Message

ui_text_input + ui_button → publishes to nodered/komut/gelen

Received Messages

mqtt in + function + ui_template → subscribes to esp32/veri/giden and formats/display messages

💬 How It Works
🔁 ESP32 → Node-RED
User types a message in the Serial Monitor

ESP32 publishes the message to esp32/veri/giden

Node-RED receives it via mqtt in node

Message is stored/formatted in a function node

ui_template displays message history on the dashboard

🔁 Node-RED → ESP32
User enters a message in Node-RED Dashboard (Send Message tab)

Clicks the “Send” button

Node-RED publishes to nodered/komut/gelen

ESP32 receives and prints it to Serial Monitor

💡 Features
🔄 Full two-way MQTT messaging

🖥 Simple, user-friendly web UI

📋 Message history using function + ui_template

📁 Clean tabbed dashboard layout

🖼 Screenshots (Add yours!)
Send Message Tab

Received Messages Tab

Node-RED Flow

Arduino Serial Monitor

🚀 Future Enhancements
Add sensors (e.g., DHT11) and visualize data with ui_chart, ui_gauge

Control actuators (LEDs, relays) from the dashboard

Secure MQTT with TLS or username/password

Log messages to a database (e.g., InfluxDB)

Enhance UI with graphs, dropdowns, styling

👤 Contributors
Caner AKCASU