Deadlight: An Interactive Two-Stage Escape Room Game System
This repository contains the full source code and configuration for "Deadlight," a multi-stage, interactive escape room game system built using two ESP32 microcontrollers, various sensors/actuators, and a centralized Node-RED control dashboard.

Project Overview
This system is designed to create an immersive, hands-on escape room experience where players must solve two main puzzles by combining physical interaction with puzzle-solving skills. The architecture is decentralized, with one ESP32 (the "Game Manager") handling the overall game flow, rules, sounds, and scoring, while a second ESP32 (the "Hardware Controller") manages the physical components like motors, an RFID reader, a microphone, and a voltmeter. The entire system can be monitored and controlled in real-time via a comprehensive Node-RED dashboard.

Core Features
Hardware and Gameplay Mechanics
Dual ESP32 Architecture: Tasks are divided between game logic and hardware control, ensuring a more stable and responsive system.
Puzzle 1 (Matrix & LED Game - ESP1):
Stage 1 (Reaction Test): A reaction-based game requiring the player to press a button at the precise moment a target light illuminates within a moving LED sequence.
Stage 2 (Memory Test - XOXO): A memory and pattern-recognition game requiring the player to replicate a pattern shown on a matrix display using four corresponding buttons.
Puzzle 2 (Physical Interaction - ESP2):
Stage 1 (Voltmeter & Motor): Requires the player to adjust a potentiometer to a specific voltage level, which then triggers a stepper motor to rotate a corresponding number of times.
Stage 2 (Microphone & Light): A puzzle where the player must make a continuous sound (like blowing) into a microphone to extinguish the light of a fully lit LED.
RFID Integration: Game transitions and specific actions are triggered by reading the correct RFID cards.
Audio Feedback: A DFPlayer Mini module provides immersive audio cues for every game phase, including startup, success, failure, transitions, and the grand finale.
Control and Management Interfaces
This project offers three distinct interfaces for seamless management by technicians and gamemasters:

ESP1 Web Interface:

Displays the live overall game status (gameState, levelDetail), remaining time, and the current text on the matrix display.
Provides a system status panel for WiFi and the Sound Module.
Features a "Settings" section to instantly adjust game difficulty parameters like animation speed, reaction window, and the number of lives.
Allows for changing the volume and the in-game audio language (TR/EN).
ESP2 Web Interface:

Shows a detailed, live status of all connected hardware: motor position and speed, button state, last read RFID UID, microphone activity, and LED brightness.
Allows for real-time customization of all motor and game parameters (steps per revolution, max speed, acceleration, mic sensitivity, etc.).
Includes manual motor control buttons for testing and calibration.
Node-RED "Game-Master" Dashboard:

Centralized Command: Provides a single-screen interface to monitor and manage the entire game flow.
Live Tracking Indicators: Live gauges and text fields for all data from both ESPs.
Full Control: Master Start and Reset buttons to manage the entire system with a single click.
Granular Skip Functions: Dedicated buttons to skip any stage of the game (Skip LED Game, Skip XOXO Game, Skip Mic Game) to advance the players if needed.
Success Icons: Visual checkmark icons that appear when each of the two main puzzles is successfully completed.
Debug Page:
Live Connection Status: Green/Red status lights indicating if each ESP is currently connected to the MQTT broker.
Direct Access: Clickable links to each ESP's native web interface for detailed hardware control.
Dual Language Support:
Game Language: A switch to change the in-game audio voiceovers between Turkish and English.
Interface Language: Buttons to change the language of the Node-RED dashboard itself (all labels and titles) between Turkish and English.
Setup and Launch
Hardware: Connect all sensors, motors, and other components to the correct pins on the ESP boards.
Arduino IDE: Upload the respective .ino and index_h.h files to each ESP board.
MQTT Broker: Ensure an MQTT broker is running at the specified IP address (192.168.20.208).
Node-RED: Import the provided flows.json file into your Node-RED instance. Verify the MQTT broker configuration in the imported flow and click Deploy.
Launch: Power on both ESP boards. Open the Node-RED dashboard at http://<your-node-red-ip>:1883/ui and start the game using the "MASTER START" button.
