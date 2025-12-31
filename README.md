# Smart Farm Hub Project

**Status:** Active Development  
**Last Updated:** Dec 2025

## 📖 Project Overview
A decentralized wireless sensor network for farm monitoring. The system consists of low-power "Spoke" nodes (sensors) communicating via ESP-NOW to a central "Hub" gateway, which uploads data to the cloud via 4G LTE.

## 🏗 Architecture
**Topology:** Star Network
- **Spokes (Sensors):** ESP8266 + Capacitive Soil Moisture Sensors.
- **Protocol (Local):** ESP-NOW (Peer-to-Peer, no Wi-Fi router required).
- **Hub (Gateway):** ESP32 + LTE Modem (SIM7600/EC200U/A7670).
- **Protocol (Cloud):** 4G LTE (HTTP GET via AT Commands) - *Integration Pending*.

---

## 🔌 Hardware Setup: The Hub (Gateway)

### Components
* **Controller:** ESP32 (DOIT DevKit V1 / NodeMCU-32S)
* **Timekeeping:** DS3231 RTC
* **Modem:** LTE CAT-1/CAT-4 Module (Quectel/SIMCom)
* **Power:** External 12V Battery (LiFePO4) -> Buck Converter -> 5V/3.3V

### 📌 Pinout Configuration
| ESP32 Pin | Modem Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **GPIO 16** | TX | UART RX | Reserved for Modem |
| **GPIO 17** | RX | UART TX | Reserved for Modem |
| **GPIO 18** | PWRKEY | Power Toggle | Reserved for Modem |
| **GND** | GND | Ground | Common ground is mandatory. |
| **VIN/5V** | VCC | Power | Modem needs high current (peak 2A). |

> **⚠️ Note:** The Modem code is currently commented out in `src-hub/src/main.cpp` while the RTC/Sleep logic is being perfected.

---

## 💻 Software Logic

### Hub (ESP32) Boot Sequence
1.  **Init RTC:** Check time against DS3231.
2.  **Window Check:** Determine if we are in an "Active Window" (XX:29-XX:34 or XX:59-XX:04).
3.  **Active Mode:** 
    *   Init ESP-NOW on Channel 1.
    *   Listen for incoming packets.
4.  **Sleep Mode:** 
    *   If outside the window, calculate seconds until next window.
    *   Enter Deep Sleep.

### Spoke (ESP8266) Logic
1.  **Wake Up:** Triggered by RTC (DS3231) or internal timer.
2.  **Read:** Analog Read (A0) with calibration logic.
3.  **Send:** Transmit struct via ESP-NOW to Hub MAC Address.
4.  **Sleep:** Deep Sleep until next "Snap-to-Grid" target (e.g., 10:00, 10:30) or hibernate until morning if after 10 PM.

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
    *   Flash to ESP32. (Modem features currently disabled).

3.  **Spoke Configuration:**
    *   Open `src-spoke/src/main.cpp`.
    *   Update `broadcastAddress[]` with your Hub's MAC Address (The Hub prints this to Serial on boot).
    *   Flash to ESP8266.

---

## 📝 CLI Reference (AT Commands)
*Note: These commands are used in the pending Modem integration.*

* `AT+CPIN?` -> Check SIM Status (Ready/Error).
* `AT+CSQ` -> Signal Quality (0-31). Aim for >15.
* `AT+CREG?` -> Network Registration (0,1 = Home, 0,5 = Roaming).
* `AT+CGDCONT=1,"IP","<APN>"` -> Set APN (e.g., `airtelgprs.com`).
* `AT+CGACT=1,1` -> Activate Data Context.
* `AT+QPING=1,"8.8.8.8"` -> Test Internet Connectivity.

---

## 🚀 Next Steps
1.  [x] **Software (Hub):** Implement ESP-NOW listener code.
2.  [ ] **Software (Hub):** Re-integrate LTE Modem code.
3.  [x] **Software (Spoke):** Migrate ESP8266 from Wi-Fi/Blynk to ESP-NOW sender.
4.  [ ] **Infrastructure:** Obtain Hub MAC Address for Spoke configuration.
