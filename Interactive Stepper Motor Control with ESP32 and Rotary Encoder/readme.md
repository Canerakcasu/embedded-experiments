# 🚀 Interactive Stepper Motor Control with ESP32 and Rotary Encoder

This project demonstrates how to precisely control a NEMA 17 stepper motor using an **ESP32**, **A4988/DRV8825 stepper driver**, and a **rotary encoder**. The rotary encoder allows interactive adjustment of the motor’s position and direction. The encoder's push-button is used to enable or disable the motor.

---

## 🎯 Project Goals

- Understand the fundamentals of stepper motor control using an ESP32.
- Learn how to interface with A4988 or DRV8825 stepper motor drivers.
- Use a rotary encoder for real-time, manual control of motor position and speed.
- Learn about current limiting and protection for stepper motors.
- Develop skills in ESP32 programming using the Arduino IDE.

---

## 🧰 Components Required

| Component                      | Description / Example                        |
|-------------------------------|----------------------------------------------|
| **Microcontroller**           | ESP32-WROOM-32 Development Board             |
| **Stepper Motor**             | NEMA 17 (e.g., 17PM-K343CN03CA)              |
| **Driver Module**             | A4988 or DRV8825                             |
| **Driver Carrier Board**      | (optional)                  |
| **Rotary Encoder**            | EC11 Module (CLK, DT, SW, +, GND)            |
| **Power Supply**              | 12V or 24V DC (minimum 1A)                   |
| **Miscellaneous**             | Jumper Wires, Multimeter, Small Screwdriver  |

---

## 🔌 Wiring Guide

### ESP32 ↔ Stepper Driver

| ESP32 Pin | Driver Pin | Function         |
|-----------|------------|------------------|
| `GND`     | `GND`      | Common Ground    |
| `GPIO 23` | `ST1`      | STEP Signal      |
| `GPIO 22` | `DIR`      | Direction Signal |
| `GPIO 21` | `EN`       | Enable Signal    |

> ℹ️ `MS1`, `MS2`, `MS3` are for microstepping and left unconnected in this project.

### ESP32 ↔ Rotary Encoder

| ESP32 Pin | Encoder Pin | Description     |
|-----------|-------------|-----------------|
| `GND`     | `GND`       | Ground          |
| `3.3V`    | `+`         | Power           |
| `GPIO 14` | `CLK`       | Clock Output    |
| `GPIO 33` | `DT`        | Data Output     |
| `GPIO 27` | `SW`        | Push Button     |

### Power Supply ↔ Stepper Driver ↔ Motor

| Connection           | Driver Pin | Purpose             |
|----------------------|------------|---------------------|
| Power `+12V`         | `VMOT`     | Motor Power Input   |
| Power `GND`          | `GND`      | Power Ground        |
| Motor Coil A         | `1A/1B`    | Motor Coil Outputs  |
| Motor Coil B         | `2A/2B`    | Motor Coil Outputs  |

> ⚠️ Make sure to correctly identify the coil pairs of the motor. If it vibrates without turning, try swapping one coil's wires.

---



## ⚙️ Current Limiting (IMPORTANT!)

Before powering your motor:

1. Disconnect the motor.
2. Power the driver with 12V.
3. Use a multimeter to measure Vref on the driver (GND to potentiometer center).
4. Adjust to limit current to about 70-80% of the motor's rated current:
   - **A4988**: ~0.6V–0.7V
   - **DRV8825**: ~0.4V–0.5V
5. Power off and reconnect the motor.

---

## 📜 How to Use

1. Connect all components and apply 12V power to the driver.
2. Connect ESP32 to PC via USB and open Serial Monitor at `115200` baud.
3. **Rotate Encoder** – Motor turns step-by-step. Fast turning = faster motor speed.
4. **Press Encoder Button** – Toggles motor state:
   - "Motor AKTIF Edildi!" → Motor activated (EN = LOW).
   - "Motor DURDURULDU!" → Motor disabled (EN = HIGH).

---

## 🧪 Troubleshooting

| Issue                          | Solution                                                                 |
|--------------------------------|--------------------------------------------------------------------------|
| Motor vibrates, doesn't rotate | Check coil wiring. Try reversing one coil. Lower speed, increase Vref. |
| Motor doesn't move at all      | Check power, wiring, EN logic level.                                     |
| Encoder doesn't respond        | Double-check CLK, DT, SW connections. Use pull-up if needed.            |

---

## 🤝 Contributing

Want to help improve this project? Contributions are welcome!  
Feel free to open an issue or submit a pull request.

---

## 📄 License

This project is open-source and available under the [MIT License](LICENSE).

