# Smart Farm Hub Project

**Status:** Active Development  
**Last Updated:** Dec 2025

## 📖 Project Overview
A decentralized wireless sensor network for farm monitoring. The system consists of low-power "Spoke" nodes (sensors) communicating via ESP-NOW to a central "Hub" gateway, which uploads data to the cloud via 4G LTE.

## 🏗 Architecture
**Topology:** Star Network
- **Spokes (Sensors):** ESP8266 + Capacitive Soil Moisture Sensors.
- **Protocol (Local):** ESP-NOW (Peer-to-Peer, no Wi-Fi router required).
- **Hub (Gateway):** ESP32 + LTE Modem (SIM7600/EC200U).
- **Protocol (Cloud):** 4G LTE (HTTP GET via AT Commands).

---

## 🔌 Hardware Setup: The Hub (Gateway)

### Components
* **Controller:** ESP32 (DOIT DevKit V1 / NodeMCU-32S)
* **Timekeeping:** DS3231 RTC
* **Modem:** LTE CAT-1/CAT-4 Module (SIM7600 / Quectel EC200U)
* **Power:** External 12V Battery (LiFePO4) -> Buck Converter -> 5V/3.3V

### 📌 Pinout Configuration
| ESP32 Pin | Modem Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **GPIO 16** | TX | UART RX | Modem RX |
| **GPIO 17** | RX | UART TX | Modem TX |
| **GPIO 18** | PWRKEY | Power Toggle | Active High/Low per modem |
| **GND** | GND | Ground | Common ground is mandatory. |
| **VIN/5V** | VCC | Power | Modem needs high current (peak 2A). |

> **✅ Note:** The Modem code is **ACTIVE** in `src-hub/src/main.cpp`. It includes specific connection logic for **Jio 4G** networks.

---

## 💻 Software Logic

### Hub (ESP32) Boot Sequence
1.  **Init RTC:** Check time against DS3231.
2.  **Modem Connect:** Initialize SIM7600/EC200U and connect to `jionet` APN.
3.  **Window Check:** Determine if we are in an "Active Window" (XX:29-XX:34 or XX:59-XX:04).
4.  **Active Mode:** 
    *   Init ESP-NOW on Channel 1.
    *   Listen for incoming packets.
    *   **Upload:** On receipt, send HTTP GET to Cloud.
5.  **Sleep Mode:** 
    *   If outside the window, calculate seconds until next window.
    *   Enter Deep Sleep.

### Spoke (ESP8266) Logic
1.  **Wake Up:** Triggered by RTC (DS3231) at scheduled time.
2.  **Read:** Analog Read (A0) with calibration logic (Dry=700, Wet=500).
3.  **Send:** Transmit struct via ESP-NOW to Hub MAC Address.
4.  **Sleep:** "Snap-to-Grid" logic calculates exact sleep seconds to hit the next :00 or :30 mark.

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
    *   Update `API_KEY` and `gcloud_url` in `main.cpp`.
    *   Flash to ESP32.

3.  **Spoke Configuration:**
    *   Open `src-spoke/src/main.cpp`.
    *   Update `broadcastAddress[]` with your Hub's MAC Address (The Hub prints this to Serial on boot).
    *   Flash to ESP8266.

---

## 📝 CLI Reference (AT Commands)
*Note: The Hub code handles these automatically, but for debugging:*

* `AT+CPIN?` -> Check SIM Status (Ready/Error).
* `AT+CSQ` -> Signal Quality (0-31). Aim for >15.
* `AT+CREG?` -> Network Registration (0,1 = Home, 0,5 = Roaming).
* `AT+CGDCONT=1,"IP","jionet"` -> Set APN.
* `AT+CGACT=1,1` -> Activate Data Context.
* `AT+QPING=1,"8.8.8.8"` -> Test Internet Connectivity.

---

## 🚀 Next Steps
1.  [x] **Software (Hub):** Implement ESP-NOW listener code.
2.  [x] **Software (Hub):** Re-integrate LTE Modem code.
3.  [x] **Software (Spoke):** Migrate ESP8266 from Wi-Fi/Blynk to ESP-NOW sender.
4.  [ ] **Infrastructure:** Obtain Hub MAC Address for Spoke configuration.
