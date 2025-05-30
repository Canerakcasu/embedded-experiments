# ESP32 ARGB LED & Matrix Control via Web Server

This project enables control of a 15-LED ARGB (NeoPixel) strip and a 4-module MAX7219 LED matrix using an ESP32 and a responsive web interface. It provides group-level color control, brightness adjustment through a rotary encoder, and text display on the matrix display.

---

## 🌟 Features

* **Wi-Fi Connectivity**

  * Static IP support or SoftAP fallback.

* **Web Interface**

  * Global ON/OFF toggle for all LEDs.
  * Global color selection.
  * Per-group ON/OFF and color for first two groups.
  * Light/Dark theme switching.
  * Display current brightness (0-100%).
  * Send scrolling text to MAX7219 LED matrix.

* **Rotary Encoder Support**

  * Adjusts global brightness.

* **ARGB LED Strip (NeoPixel)**

  * 15 LEDs divided into 5 groups of 3 LEDs.
  * First two groups controllable individually.

* **MAX7219 LED Matrix Display**

  * Displays messages entered via web UI.

---

## 🛠️ Hardware Requirements

* ESP32 Dev Board
* WS2812B (NeoPixel) LED Strip (15 LEDs)
* Rotary Encoder Module (CLK, DT, SW, VCC, GND)
* MAX7219 Matrix Display (4 chained modules)
* Breadboard & Jumper Wires
* **10K Ohm Pull-up Resistors** (for encoder CLK, DT, and SW if needed)
* **5V Power Supply** (for LED strip and matrix)
* ESP32 Power via Micro USB

---

## 💻 Software Requirements

* Arduino IDE (with ESP32 board package)
* Required Libraries:

  * `WiFi.h`, `WebServer.h`, `SPI.h` (ESP32 Core)
  * `Adafruit_NeoPixel` by Adafruit
  * `ESP32Encoder` by madhephaestus
  * `MD_Parola`, `MD_MAX72xx` by majicDesigns

---

## 🔌 Wiring Guide

**1. ARGB LED Strip (GPIO 33)**

* DIN: GPIO 33
* VCC: External 5V
* GND: ESP32 GND + External GND

**2. Rotary Encoder**

* CLK: GPIO 32 + 10K Pull-up
* DT: GPIO 35 + 10K Pull-up
* SW: GPIO 34 + 10K Pull-up
* VCC: 3.3V
* GND: ESP32 GND

**3. MAX7219 Matrix**

* VCC: External 5V
* GND: ESP32 GND + External GND
* DIN: GPIO 12
* CS: GPIO 13
* CLK: GPIO 2

---

## ⚙️ Arduino Setup

1. Open the `.ino` sketch.

2. **Wi-Fi Setup:**

   * Edit `ssid` and `password`.
   * Adjust static IP if needed (`WiFi.config(...)`).

3. **Verify Pin Assignments:**

   * ARGB LED: GPIO 33
   * Encoder: CLK 32, DT 35, SW 34
   * Matrix: CLK 2, DATA 12, CS 13

4. **Upload Code** to ESP32.

---

## 🚀 How to Use

### Power Up

Connect ESP32, LED strip, and matrix to appropriate power.

### Connect to Wi-Fi

* ESP32 attempts Wi-Fi connection.
* On failure, it starts fallback AP: `ESP32-FallbackAP`, password: `12345678`.

### Get IP Address

* Open Serial Monitor (115200 baud).
* Note the IP (e.g., `192.168.20.21` or fallback `192.168.4.1`).

### Open Web UI

* Visit IP in browser.

### Control Panel

**General Settings**

* Toggle all LEDs ON/OFF
* Switch theme (light/dark)
* Set global color
* View brightness (from rotary encoder)

**Group Control (Group 1 & 2)**

* Toggle ON/OFF individually
* Set individual colors

**Matrix Display**

* Input text → click *"Gönder"* → message scrolls on MAX7219

**Rotary Encoder**

* Rotate to adjust global brightness

---

## ⚠️ Troubleshooting

* **Encoder Issues:** Use 10K pull-ups for GPIO 32, 34, and 35.
* **Power Draw:** Use adequate 5V supply. Connect GNDs.
* **Wi-Fi:** Ensure correct SSID/password. Static IP must be network-compatible.
* **Web UI Timeout:** Confirm device is on same network. Check IP and firewall settings.

---

## 📂 Project Files

* `ESP32_ARGB_Matrix_WebControl.ino` (main sketch)
* `README.md` (this file)

---

Enjoy your interactive ARGB & matrix project!
