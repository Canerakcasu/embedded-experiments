# 🔥 Deadlight Game System

An interactive escape-room-style reflex and memory game built with ESP32, combining LED strip visuals, matrix screen puzzles, sound effects, and a web interface for configuration.

## 🎮 Overview

Deadlight is a 10-minute physical-interaction game where players must react quickly and remember visual codes. The game is divided into two steps:

1. **Red Light Reflex**  
   Players must hit a button exactly when a red LED lights up during a fast-moving green progress bar.

2. **Memory Challenge**  
   Players must replicate 4-character code sequences (like `OXOX`) shown briefly on a matrix display using four physical buttons.

If players complete both steps, a success sequence is triggered. If they fail within the 10-minute window, the game ends in failure.

## 🧠 Features

- 🎯 Reflex-based LED challenge (progress bar + timed red light)
- 🔢 LED Matrix memory code repetition game
- 🎵 DFPlayer Mini-based sound effects and voice messages
- 🌐 Web-based ESP32 interface (Dark mode with red accents)
- 🛠️ Adjustable settings (volume, speed, language)
- 📊 Node-RED status integration for remote monitoring

## ⚙️ Hardware Required

- ESP32 DevKit
- DFPlayer Mini + Speaker
- WS2812B (15 LEDs)
- MAX7219 LED Matrix (controlled via DIN, CLK, CS)
- 4 push buttons (for memory game)
- 1 button (for reflex step)
- SD Card with audio files named `0001.mp3`, `0002.mp3`, `0003.mp3`, `0004.mp3`

## 🖥️ Web Interface

- Built-in ESP32 webserver
- Theme: Dark with red highlights
- Control panel:
  - View hardware connection status
  - Start/reset game
  - Adjust volume and LED speed
  - Change language (English / Polish for UI, English / Turkish for audio)

## 🧪 Game Flow

### Step 1: Reflex Challenge

- LED strip starts animating from left to right in green.
- At a random point, a red LED blinks.
- Player must press the reflex button at the exact moment.
- Each correct hit activates a letter in the matrix spelling `D-E-A-D`.
- After 4 successful hits, a voice says:
  
  > “HAHA you are one step closer to death”

- `0001.mp3` is played at game start  
- `0002.mp3` is played after Step 1 ends

### Step 2: Memory Game

- Matrix briefly shows a code (e.g., `OXOX`) for 1 second.
- Screen clears and waits (`- - - -`) for 5 seconds.
- Player presses buttons corresponding to `X` only.
- Correct input progresses to next level, wrong input triggers `Error` sound and repeat.

After 4 levels, `0004.mp3` is played and green LEDs blink slowly to indicate success.

## 📂 Audio Files (SD Card)

| Filename   | Description                    |
|------------|--------------------------------|
| `0001.mp3` | Game start message             |
| `0002.mp3` | Step 1 success / "you’re close"|
| `0003.mp3` | Step 2 intro                   |
| `0004.mp3` | Game completion / success      |
| `error.mp3`| Played on wrong input          |

## 🌐 Node-RED Integration

- WebSocket or MQTT (customizable)
- Shows:
  - Active buttons
  - Game steps completed
  - Timer countdown
  - Error or success states

## 📦 Project Structure

DeadlightGame/
├── data/ # Web interface files (HTML, CSS, JS)
├── src/
│ └── main.ino # Main Arduino sketch
├── README.md


## 🧰 Libraries Used

- Adafruit NeoPixel
- LedControl
- DFRobotDFPlayerMini
- WebServer
- SPIFFS
- ArduinoJson (optional)
- WiFi

## 🏁 Getting Started

1. Flash `main.ino` to your ESP32.
2. Format and upload web files to SPIFFS using Arduino IDE or PlatformIO.
3. Connect all hardware.
4. Power on ESP32 and access the web interface via its IP.