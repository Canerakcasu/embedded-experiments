# 🔄 ESP32 Dual-Core Projects with FreeRTOS (Arduino)

This repository contains **two dual-core ESP32 projects** built using the **Arduino framework** and **FreeRTOS**. Each project demonstrates how to manage **parallel tasks**, **interrupts**, **critical sections**, and **inter-core communication**.

---

## 📁 Project 1: LED Control with Button Interrupt (Dual-Core Task Management)

### 📝 Description

This project demonstrates **task management and inter-task communication** across ESP32's dual cores. It uses:
- A blinking LED task on **Core 0**
- A button monitoring task on **Core 1**
- A **semaphore** to signal from an **ISR (Interrupt Service Routine)** to a task
- **vTaskSuspend() / vTaskResume()** to control task execution

### 🧠 Core Roles

- **Core 0**: Runs `ledTask`, which continuously toggles an LED (GPIO 33).
- **Core 1**: Runs `buttonTask`, which responds to a button press (GPIO 2) using an interrupt.

### 🔧 How it Works

1. `setup()` creates two tasks pinned to different cores.
2. A binary semaphore is created and given from ISR when the button is pressed.
3. The `buttonTask` toggles `ledTask`'s state:
   - **Suspends** the task and turns off the LED.
   - **Resumes** the task to start blinking again.

### 🪛 Hardware

| Component | Connection |
|----------|-------------|
| LED      | Anode → Resistor (220–330Ω) → GPIO 33<br>Cathode → GND |
| Button   | One leg → GPIO 2<br>Other leg → GND |

> Note: GPIO 2 is configured with `INPUT_PULLUP`.

### 🖥️ Example Serial Output

[Core 1] Button interrupt attached.
[Core 1] Button Task started.
[Core 1] Setup finished.
[Core 0] LED Task started.
[Core 1] Button press detected!
[Core 1] LED Task Suspended.
[Core 1] Button press detected!
[Core 1] LED Task Resumed.



---

## 📁 Project 2: Shared Variable Management with Critical Sections

### 📝 Description

This project demonstrates **safe manipulation of a shared variable (`counter`)** using **critical sections** (mutexes) to prevent race conditions across tasks.

### ⚙️ Tasks

- **`task1()`**: Increments the counter by **1** every 1 second.
- **`task2()`**: Increments the counter by **1000** every 500 ms.
- Both tasks use a **mutex (`portMUX_TYPE`)** to avoid simultaneous access.

### 🧠 Core Roles

Both tasks run on **Core 0**, but can be adapted to run on different cores.

### 🧪 Purpose

- Shows **race condition prevention** using:
  ```cpp
  portENTER_CRITICAL(&taskMux);
  // critical code
  portEXIT_CRITICAL(&taskMux);


### 🖥️ Example Serial Output

Task 1: Trying to increment the counter at 1245
Task 1: Counter = 1001
Task 2: Trying to increment the counter at 1502
Task 2: Counter = 2001


🔧 Requirements
ESP32 board support in Arduino IDE

FreeRTOS (included with ESP32 Arduino core)

Arduino IDE or PlatformIO

📦 Installation & Upload
Install ESP32 board package in Arduino IDE.

Connect your ESP32 board.

Select the correct board and COM port.

Open the desired project.

Upload and open Serial Monitor at 115200 baud.

