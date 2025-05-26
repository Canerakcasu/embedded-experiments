# ESP32 NTP Time Synchronization Example 🕒

This project demonstrates how to connect an ESP32 development board to a Wi-Fi network and retrieve the current time and date from Network Time Protocol (NTP) servers. The obtained time is displayed on the Serial Monitor as local time, supporting time zone and Daylight Saving Time (DST) adjustments.

---

## 📋 Features

- Connects the ESP32 to a Wi-Fi network
- Synchronizes time using multiple NTP servers
- Utilizes the built-in Simple Network Time Protocol (SNTP) client
- Supports configuration of Time Zone and Daylight Saving Time
- Triggers a callback function when time is successfully obtained
- Displays the local time periodically on the Serial Monitor

---

## ⚙️ Hardware Requirements

- Any ESP32 Development Board (e.g., ESP32-DevKitC, NodeMCU-32S, etc.)

---

## 💻 Software Requirements

- [Arduino IDE](https://www.arduino.cc/en/software)
- [ESP32 Core for Arduino IDE](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)  
  (Ensure the `WiFi.h` and ESP SNTP libraries are available)

---

## 🔧 Configuration

Before uploading the code to your ESP32, configure the following variables in the `.ino` file:

### 1. Wi-Fi Credentials
Replace with your actual network credentials:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

2. NTP Servers (Optional)
The defaults are:

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
You can change them if needed.

3. Time Zone & DST
Adjust based on your location:

const long gmtOffset_sec = 3600;             // Example: UTC+1
const int daylightOffset_sec = 3600;         // Example: 1 hour DST
const char *time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Example: Europe/Rome
You can use POSIX TZ format for better DST handling.

▶️ How to Use
Open the code in your Arduino IDE

Configure Wi-Fi credentials and time zone settings

Select your ESP32 board and COM port under Tools

Upload the code to the ESP32

Open Serial Monitor (Ctrl+Shift+M)

Set baud rate to 115200

Watch the ESP32 connect and print the local time every 5 seconds

💡 How it Works
Setup
Initializes Serial communication

Connects to Wi-Fi

Sets up SNTP and a callback function timeavailable()

Configures time using configTime() or configTzTime()

Loop
Waits 5 seconds

Calls printLocalTime() to print the current time

printLocalTime()
Retrieves time using getLocalTime()

If successful, prints formatted time

Otherwise, prints "No time available"

timeavailable()
Called once when NTP time is successfully synced

Prints a confirmation message

🤝 Contributing
Feel free to fork this repository, make improvements, and submit pull requests!

📜 License
This project is open-source. Feel free to use and modify it. Attribution is appreciated but not required.

