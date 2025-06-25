# ESP32 Web-based SD Card MP3 Player 🎵

This project is a comprehensive **media server** built with an **ESP32 development board**, allowing you to manage and play MP3 files from an SD card via a sleek **web interface** over your local network.

---

## ✨ Features

- **Web-Based File Management**  
  Upload, delete, and rename files directly through the browser.

- **Asynchronous File Upload**  
  Uploads happen in the background without reloading the page.

- **Real-Time Status Panel**  
  Displays SD card status (Ready/Error), current track, and volume level.

- **Audio Control**  
  Volume adjustment and playback stop functionality included.

- **Modern Dark Theme UI**  
  Mobile-friendly, responsive design with a clean look.

- **Robust Error Handling**  
  Advanced debug info and fixes for common ESP32 issues, such as memory corruption and SPI conflicts.

---

## 🧰 Hardware Requirements

| Component             | Description                                   |
|----------------------|-----------------------------------------------|
| ESP32 Dev Board      | e.g., NodeMCU-32S, DOIT DevKit V1             |
| MicroSD Card Module  | SPI interface required                        |
| MAX98357A Module     | I2S mono amplifier + DAC (3W)                 |
| Passive Speaker      | 4–8 Ohm, min. 3W                              |
| MicroSD Card         | FAT32 formatted, high-quality brand preferred |
| Jumper Wires + Breadboard | For prototyping and connections       |

---

## 📦 Required Libraries (Arduino IDE)

- `ESPAsyncWebServer`  
- `AsyncTCP`  
- `ESP8266Audio` by **Earle F. Philhower, III** (Yes, works with ESP32)

---

## 🪛 Wiring Diagram (Stable & Tested Configuration)

Using the standard **VSPI** port for the SD card:

| Module Pin   | ESP32 Pin | Description              |
|--------------|-----------|--------------------------|
| SD MOSI      | GPIO 23   | SPI Data Out             |
| SD MISO      | GPIO 19   | SPI Data In              |
| SD SCK       | GPIO 18   | SPI Clock                |
| SD CS        | GPIO 5    | SPI Chip Select          |
| AMP DIN      | GPIO 22   | I2S Data                 |
| AMP BCLK     | GPIO 26   | I2S Bit Clock            |
| AMP LRC      | GPIO 25   | I2S Word Select (L/R)    |
| Module VCC   | 5V (VIN)  | Power Supply             |
| Module GND   | GND       | Common Ground            |

---

## 🚀 Installation & Usage

1. Install required libraries in the **Arduino IDE**.
2. Open the `.ino` file and update your Wi-Fi SSID and password.
3. Upload the code to the ESP32 board.
4. Open **Serial Monitor** at `115200 baud`.
5. Once connected to Wi-Fi, note the **IP address** printed.
6. Access the player via browser using that IP (on same local network).
7. Enjoy uploading, playing, and managing your MP3 files!

---

## 🛠️ The Project Journey: Problems & Solutions

### 🧩 Problem 1: Compilation Errors - Wrong "Audio" Library

- **Symptom**: `Audio` class not recognized.
- **Diagnosis**: Wrong "Audio" library (Arduino's own for ARM) was installed.
- **Solution**: Uninstall incorrect one and install `ESP8266Audio` by Earle F. Philhower, III.

---

### ⚡ Problem 2: Hardware Instability & Upload Errors

- **Symptom**: Simple sketches (e.g., Blink) failed to upload.
- **Diagnosis**: Faulty ESP32 board or bad USB cable.
- **Solution**: Replaced the ESP32 board and USB cable. Stable upload resumed.

---

### 📡 Problem 3: SD Card Conflicts with Active Wi-Fi

- **Symptom**: SD works in basic sketches but fails with Wi-Fi + WebServer active.
- **Diagnosis**: SPI bus conflict due to Wi-Fi using the same pins.
- **Tried**: Switching SD card to HSPI (custom SPI pins), but GPIO12 caused boot issues.
- **Final Fix**: Used **a new ESP32 board** with reliable **standard VSPI pins**, which resolved conflict.

---

### 🕳️ Problem 4: "Missing Zero" Bug (Memory Corruption)

- **Symptom**: Files like `004.mp3` show up as `04.mp3` after upload.
- **Diagnosis**: Runtime memory corruption when multiple systems run together (Wi-Fi, Audio, Server).
- **Fix (Workaround)**:
  - Files are saved with an `x` prefix, e.g., `x004.mp3`.
  - Web interface **hides** this prefix.
  - When playing, renaming, or deleting, the ESP32 **adds the prefix back** internally.
  - Result: User sees normal filenames, system avoids corrupted memory access.

---

## 📸 Screenshots 



---

## 📄 License

MIT License – free to use, modify, and share.

---

## 🙌 Credits

Developed by Caner AKCASU 
Inspired by open-source communities and the power of ESP32.

---
