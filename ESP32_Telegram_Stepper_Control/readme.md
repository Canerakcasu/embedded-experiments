# ESP32 Telegram Stepper Control

Control a stepper motor using an ESP32 microcontroller through Telegram bot commands and a local rotary encoder interface.

## 🚀 Features

- **Telegram Bot Control:**
  - `motoron` / `motoroff` — Enable/disable the stepper driver.
  - `turn <degrees>` — Rotate the motor by specific degrees (e.g., `turn 90`, `turn -45`).
  - `step <steps>` — Rotate by a specific number of steps (e.g., `step 200`, `step -100`).
  - `motorstatus` — Query driver enable status.

- **Local Control via Rotary Encoder:**
  - Rotate motor step-by-step using encoder knob.
  - Toggle motor enable/disable with encoder push button.

- **Security:**
  - Only a predefined Telegram `CHAT_ID` is authorized to control the system.

---

## 🧰 Hardware Requirements

- **Microcontroller:** ESP32 board.
- **Stepper Motor:** Bipolar/unipolar stepper motor.
- **Driver:** A4988 / DRV8825 / compatible stepper driver.
- **Rotary Encoder:** e.g., KY-040 module.
- **Power Supply:**
  - ESP32 via USB or external 5V/3.3V.
  - Dedicated power supply for the stepper motor.
- **Optional:** Breadboard, jumper wires.

---

## 📦 Libraries & Software

- **Arduino IDE**
- **Libraries:**
  - `WiFi.h`
  - `WiFiClientSecure.h`
  - `UniversalTelegramBot` by Brian Lough
  - `AiEsp32RotaryEncoder` by Aircookie

---

## ⚙️ Configuration

Update the following parameters in `ESP32_Telegram_Stepper_Control.ino` before uploading:

```cpp
// WiFi Credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Telegram Bot
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"

// Stepper Driver Pins
#define STEP_PIN 23
#define DIR_PIN 22
#define EN_PIN 21

// Motor Parameters
#define STEPS_PER_REVOLUTION 200
#define MICROSTEPPING_FACTOR 1

// Rotary Encoder Pins
#define ROTARY_ENCODER_A_PIN 14
#define ROTARY_ENCODER_B_PIN 33
#define ROTARY_ENCODER_BUTTON_PIN 27
#define ROTARY_ENCODER_STEPS 4

// Optional: Stepper Speed
int stepMotorSpeedDelay = 1000; // Microseconds between step pulses

🔌 Wiring Overview
🌀 Stepper Driver ↔ ESP32:
STEP → GPIO 23

DIR → GPIO 22

EN → GPIO 21

Driver GND/VCC → ESP32 GND/3.3V (or 5V depending on logic level)

⚙️ Stepper Motor:
Connect coils to driver as per datasheet.

🔄 Rotary Encoder ↔ ESP32:
CLK → GPIO 14

DT → GPIO 33

SW → GPIO 27

VCC → 3.3V or 5V

GND → GND

⚡ Power:
ESP32 powered via USB.

Stepper driver powered by separate motor supply.

All grounds (ESP32, encoder, driver logic & motor) must be common.

🧪 Usage
Hardware Setup: Wire everything as described.

Software Prep: Set parameters, install libraries, compile.

Upload Sketch: Use Arduino IDE.

Monitor Serial Output: Open Serial Monitor @ 115200 baud.

Telegram Commands:

/start — See available commands.

motoron / motoroff

turn 90, turn -45

step 200, step -50

motorstatus

Rotary Encoder:

Press button to toggle motor driver.

Rotate knob to step motor when enabled.

🛠️ Troubleshooting
No Telegram Response:

Check BOT_TOKEN, CHAT_ID, Wi-Fi.

'''
Make sure youve started a conversation with your bot.

Test bot token:
https://api.telegram.org/bot<YOUR_BOT_TOKEN>/getMe

Motor Doesn’t Move:

Check wiring & power.

Ensure driver is enabled (motoron).

Verify STEP_PIN, DIR_PIN.

Rotary Encoder Not Working:

Check encoder wiring.

Ensure ROTARY_ENCODER_STEPS is accurate.

📌 Notes
Adjust microstepping factor and speed for your specific setup.

EN pin logic may vary by driver (LOW = enabled for A4988/DRV8825).

Consider debouncing encoder inputs if noisy.
'''
