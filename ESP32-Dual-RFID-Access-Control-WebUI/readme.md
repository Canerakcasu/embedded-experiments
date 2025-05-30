# RFID Dual Reader Access System with ARGB LED Feedback

This project uses two MFRC522 RFID readers connected to an ESP32. Each reader has two authorized cards. When a card is scanned, the system checks if it's allowed and gives visual feedback using a 15-pixel ARGB LED strip.

## Features

- **Two RFID readers** (MFRC522)
- **Each reader supports 2 authorized cards**
- **Visual feedback using ARGB LEDs:**
  - ✅ **Blue blink** for authorized cards
  - ❌ **Red blink** for unauthorized cards

## Hardware

- ESP32
- 2x MFRC522 RFID readers
- 15-pixel ARGB LED strip (connected to GPIO 2)
- SPI connections for RFID modules

## Pin Configuration

- **Reader 1:** SS = GPIO 15, RST = GPIO 22
- **Reader 2:** SS = GPIO 4, RST = GPIO 21
- **LED Strip:** GPIO 2

## How It Works

1. On boot, the system initializes the LED strip and RFID readers.
2. When a card is presented:
   - Its UID is read and compared with the list of allowed UIDs.
   - If it matches: the LED strip blinks **blue** 3 times.
   - If not: the LED strip blinks **red** 3 times.
3. Serial output shows UID and result (matched or not).

## Notes

- You can change the allowed UIDs in the `allowedUIDs_Reader1` and `allowedUIDs_Reader2` arrays.
- Ensure proper SPI wiring for stable RFID communication.

