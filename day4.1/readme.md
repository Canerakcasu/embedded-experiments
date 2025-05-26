# ESP32 Dual-Core Demonstration Projects ⚙️🧠

This repository contains **two example projects** that demonstrate the **dual-core capabilities** of the **ESP32** using the **Arduino framework** and **FreeRTOS**.

---

## 📁 Project 1: Basic Dual-Core Task Demonstration

### 📝 Description

This project demonstrates how to run **separate tasks on each core** of the ESP32.

- **Core 0**: Runs a dedicated task (`Task1code`) that continuously prints a message to the Serial Monitor.
- **Core 1**: Runs the standard `setup()` and `loop()` functions. The `loop()` also continuously prints messages, showing that it's running on Core 1.

### ⚙️ How it Works

- `setup()` initializes Serial communication and creates a new task (`Task1code`) using `xTaskCreatePinnedToCore()`, assigning it to **Core 0**.
- `Task1code()` runs in an infinite loop and prints its status and the core ID.
- `loop()` (which runs on **Core 1**) also prints messages continuously.

### 🧪 Outcome

This project helps **visualize the concurrent execution** of code on both ESP32 cores via Serial output.

---

## 📁 Project 2: Dual-Core LED Control with Button Interrupt

### 📝 Description

This project demonstrates **interactive dual-core usage** with:

- **Background LED blinking**
- **Button interrupt**
- **Semaphore-based inter-task communication**

### 💡 Core Assignments

- **Core 0** (`ledTask`):
  - Blinks an LED connected to **GPIO 33** (`LED_PIN`)
  - Prints status messages to Serial Monitor

- **Core 1** (`buttonTask`):
  - Monitors a button connected to **GPIO 2** (`BUTTON_PIN`)
  - Uses a hardware interrupt to detect button presses
  - Uses a semaphore to safely notify the button task
  - **Toggles the `ledTask`** between suspended and resumed states

### 🔁 Task Interaction

- On **button press**:
  - If LED task is running → `vTaskSuspend()` it
  - If LED task is suspended → `vTaskResume()` it
  - Logs actions via Serial (e.g., `"Button press detected!"`, `"LED Task Suspended"`)

### ⚙️ How it Works

- `setup()`:
  - Initializes pin modes
  - Creates a **binary semaphore**
  - Attaches **interrupt** to the button
  - Creates and pins:
    - `ledTask` → Core 0
    - `buttonTask` → Core 1

- `button_ISR()`:
  - Triggered on button press
  - Calls `xSemaphoreGiveFromISR()` to notify the task

- `buttonTask()`:
  - Waits on the semaphore
  - Toggles `ledTask` accordingly

- `ledTask()`:
  - Continuously blinks the LED (if running)

---

## 🔑 Key Features Demonstrated

- **Dual-Core Operation**: Tasks pinned to specific cores
- **FreeRTOS Tasks**: `xTaskCreatePinnedToCore()` usage
- **Interrupts**: `attachInterrupt()` for responsive input
- **Semaphores**: For communication between ISR and task
- **Task Control**: `vTaskSuspend()` and `vTaskResume()`
- **Debugging**: `Serial.println()` with `xPortGetCoreID()` to display the core origin of messages

---

## 🔌 Hardware Connections

### 🟢 LED
- Anode (+) → Resistor (220–330Ω) → **GPIO 33**
- Cathode (−) → GND

### 🔴 Button
- One leg → **GPIO 2**
- Other leg → GND  
  (Configured with `INPUT_PULLUP`)

---

## ▶️ How to Use

1. Make sure you have the **ESP32 board package** installed in the Arduino IDE.
2. Open the desired project.
3. Select your **ESP32 board** under `Tools > Board`.
4. **Connect hardware**:
   - Project 1: No hardware required
   - Project 2: Connect LED and button as described above
5. Upload the code.
6. Open **Serial Monitor** at `115200 baud` and observe dual-core behavior.

---

## 🖼️ Example Serial Monitor Output

