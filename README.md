# SenseAir S88 Async Library

![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-orange)
![Arduino](https://img.shields.io/badge/Arduino-compatible-blue)
![License](https://img.shields.io/badge/license-MIT-green)

A highly reliable, asynchronous, non-blocking C++ library for the SenseAir S8 and S88 CO2 sensors. Built for industrial IoT and smart home applications where main loop blocking is unacceptable.

## 🚀 Key Features
* **100% Non-blocking (State Machine):** Never uses `delay()`. Keeps your MCU free for WiFi, MQTT, and UI tasks.
* **Asynchronous Hardware Calibration (v1.1.0+):** Safely perform the 30-second background CO2 calibration (400 ppm) completely asynchronously without freezing your device.
* **Command Queueing:** Uses a Ring Buffer to queue multiple requests (`requestCO2()`, `setABCPeriod()`).
* **Self-Healing:** Automatically flushes dropped/corrupted requests after consecutive errors but retains critical EEPROM write commands.
* **Strict Validation:** Real-time CRC16 calculation and boundary checks (400 - 10000 ppm) protect against noisy hardware lines.
* **Callback Driven:** Notifies your main loop instantly when valid data arrives or when a command fails.

## 🛠 Wiring
| SenseAir S8/S88 Pin | Arduino Pin |
| :--- | :--- |
| G+ (Power) | 5V |
| G0 (Ground) | GND |
| TxD | Rx (e.g., D10 for SoftwareSerial) |
| RxD | Tx (e.g., D11 for SoftwareSerial) |

## 📦 Installation

**PlatformIO:** Add the following to your `platformio.ini` file:
```ini
lib_deps = specleds/SenseAir_S88_Async
