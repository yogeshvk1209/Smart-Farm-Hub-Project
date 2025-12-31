# Smart Farm Hub Project

**Status:** In Development  
**Last Updated:** Dec 2025

## 📖 Project Overview
A decentralized wireless sensor network for farm monitoring. The system consists of low-power "Spoke" nodes (sensors) communicating via ESP-NOW to a central "Hub" gateway, which uploads data to the cloud via 4G LTE.

## 🏗 Architecture
**Topology:** Star Network
- **Spokes (Sensors):** ESP8266 + Capacitive Soil Moisture Sensors.
- **Protocol (Local):** ESP-NOW (Peer-to-Peer, no Wi-Fi router required).
- **Hub (Gateway):** ESP32 + LTE Modem (SIM7600/EC200U/A7670).
- **Protocol (Cloud):** 4G LTE (HTTP GET via AT Commands).

---

## 🔌 Hardware Setup: The Hub (Gateway)

### Components
* **Controller:** ESP32 (DOIT DevKit V1 / NodeMCU-32S)
* **Modem:** LTE CAT-1/CAT-4 Module (Quectel/SIMCom)
* **Power:** External 12V Battery (LiFePO4) -> Buck Converter -> 5V/3.3V

### 📌 Pinout Configuration
| ESP32 Pin | Modem Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **GPIO 16** | TX | UART RX | ESP32 receives data here |
| **GPIO 17** | RX | UART TX | ESP32 transmits data here |
| **GPIO 18** | PWRKEY | Power Toggle | **Do NOT use RST.** Pulse LOW to toggle power. |
| **GND** | GND | Ground | Common ground is mandatory. |
| **VIN/5V** | VCC | Power | Modem needs high current (peak 2A). |

> **⚠️ Critical Warning:** Do not use **GPIO 2 (D4)**, **GPIO 0**, or **GPIO 12** for the Modem PWRKEY. These pins toggle during boot ("Strapping Pins") and will inadvertently turn off the modem.

---

## 💻 Software Logic

### Hub (ESP32) Boot Sequence
1.  **Init Serial:** Start `HardwareSerial(2)` at 115200 baud.
2.  **Modem Check:** Send `AT`.
    * If `OK`: Modem is running. Proceed.
    * If Timeout: Pulse **GPIO 18** LOW for 2000ms. Wait 5s for boot.
3.  **Network Check:**
    * `AT+CSQ` (Check Signal Strength > 10).
    * `AT+CREG?` (Check Network Registration).
4.  **Data Loop:** Listen for ESP-NOW packets -> Upload via LTE.

### Spoke (ESP8266) Logic
1.  **Wake Up:** Triggered by RTC (DS3231) or internal timer.
2.  **Read:** Analog Read (A0) with calibration logic.
3.  **Send:** Transmit struct via ESP-NOW to Hub MAC Address.
4.  **Sleep:** Deep Sleep until next scheduled wake time (Day interval or next morning).

---

## 🛠 Getting Started

### Prerequisites
*   **VS Code** with **PlatformIO** extension installed.
*   **Google Cloud Account** (for Backend).

### Setup Instructions

1.  **Clone the Repository:**
    ```bash
    git clone https://github.com/your-username/Smart-Farm-Hub-Project.git
    ```

2.  **Hub Configuration:**
    *   Navigate to `src-hub/src/`.
    *   Rename `secrets_example.h` to `secrets.h`.
    *   Enter your Google Cloud Run URL in `secrets.h`.
    *   Flash to ESP32.

3.  **Spoke Configuration:**
    *   Open `src-spoke/src/main.cpp`.
    *   Update `broadcastAddress[]` with your Hub's MAC Address (The Hub prints this to Serial on boot).
    *   Flash to ESP8266.

---

## 📝 CLI Reference (AT Commands)
Useful commands for debugging the modem via "Passthrough" sketch:

* `AT+CPIN?` -> Check SIM Status (Ready/Error).
* `AT+CSQ` -> Signal Quality (0-31). Aim for >15.
* `AT+CREG?` -> Network Registration (0,1 = Home, 0,5 = Roaming).
* `AT+CGDCONT=1,"IP","<APN>"` -> Set APN (e.g., `airtelgprs.com`).
* `AT+CGACT=1,1` -> Activate Data Context.
* `AT+QPING=1,"8.8.8.8"` -> Test Internet Connectivity.

---

## 🚀 Next Steps
1.  [ ] **Hardware:** Source replacement SIM card.
2.  [ ] **Software (Hub):** Implement ESP-NOW listener code.
3.  [ ] **Software (Spoke):** Migrate ESP8266 from Wi-Fi/Blynk to ESP-NOW sender.
4.  [ ] **Infrastructure:** Obtain Hub MAC Address for Spoke configuration.
