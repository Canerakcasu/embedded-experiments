# ESP_TeleStrip_Bot

Control your ARGB LED strip (WS2812B/NeoPixel type) using an ESP32 or ESP8266 microcontroller via a Telegram bot. This project allows you to turn the strip on/off, set specific RGB colors, and adjust brightness remotely.

## Features

- ✅ Turn ARGB LED strip ON/OFF  
- 🎨 Set custom RGB colors for the entire strip  
- 🌗 Adjust overall brightness of the strip  
- 📟 Get the current status of the strip (ON/OFF, color, brightness)  
- 💬 Control via Telegram bot commands (case-insensitive for simple commands)  
- 🔒 Authorized user control using `CHAT_ID`  

## Hardware Needed

- **Microcontroller:** ESP32 or ESP8266 development board  
- **ARGB LED Strip:** WS2812B, SK6812, NeoPixel, or similar (configured for `15` LEDs)  
- **Logic Level Shifter (Recommended):** For converting ESP’s 3.3V data to 5V (e.g., 74AHCT125)  
- **Power Supply:**
  - A stable 5V for the LED strip (e.g., USB adapter, battery pack)
  - ⚠️ Each LED can draw up to 60mA at full white brightness. For 15 LEDs = up to 900mA!
  - **Do NOT power the LED strip directly from the ESP's 5V pin**
- **Jumper Wires / Breadboard (optional)**

## Software & Libraries

- **Arduino IDE**
- Required Libraries:
  - `WiFi.h` or `ESP8266WiFi.h`  
  - `WiFiClientSecure.h`  
  - `UniversalTelegramBot.h` by Brian Lough  
  - `FastLED.h`  
  - `stdio.h` (standard with ESP core)

## Configuration

Update the following in your `.ino` file before uploading:

```cpp
const char* WIFI_SSID = "Your_WiFi_SSID";
const char* WIFI_PASSWORD = "Your_WiFi_Password";

#define BOT_TOKEN "Your_Telegram_Bot_Token"
#define CHAT_ID "Your_Numerical_Chat_ID"

#define ARGB_LED_PIN 2     // GPIO for LED data
#define TOTAL_LEDS 15      // Number of LEDs
```
![IMG_0621](https://github.com/user-attachments/assets/a3af9877-d6f4-46dc-8fea-12159a1380a0)

📌 Tip: Use a logic level shifter if your strip needs 5V logic.

Usage
Wire the hardware as described

Update and upload code to your ESP board

Open Serial Monitor @ 115200 baud

Send commands to your bot on Telegram:

Telegram Commands
Command	Description
/start	Show available commands
/strip_on or ledon	Turn strip ON
/strip_off or ledoff	Turn strip OFF
/rgb R G B	Set color (e.g. /rgb 255 0 0 = red)
/brightness VALUE	Brightness (0-255)
/strip_status or ledstatus	Show current status

Commands are case-insensitive for simple ones (ledon, ledoff, etc.).

Troubleshooting
No response / No LED action:

Check BOT_TOKEN, CHAT_ID, Wi-Fi credentials

Make sure bot is started and you've sent a message to it

Power supply sufficient? GND connected properly?

Test BOT token: https://api.telegram.org/bot<BOT_TOKEN>/getMe

LEDs flicker / wrong color:

Use logic level shifter

Check LED count matches TOTAL_LEDS

Avoid long, unstable jumper wires
