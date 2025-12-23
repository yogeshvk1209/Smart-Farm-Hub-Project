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
- **Protocol (Cloud):** 4G LTE (MQTT / Blynk) over UART.

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

## 🛠 Troubleshooting Log (Lessons Learned)

### Issue 1: Modem "Double Toggle" Loop
* **Symptom:** Modem turns ON when power is applied, but turns OFF exactly when ESP32 resets.
* **Cause:** The ESP32 code was blindly pulsing the PWRKEY LOW for 2 seconds on setup. Since the modem was *already* ON, this pulse acted as a "Shutdown" command.
* **Fix:** Implemented "Check-First" logic. Code now sends `AT` command first. If `OK` is received, it skips the power pulse.

### Issue 2: The "Boot Blink" Trap
* **Symptom:** Modem powers off on boot even without code logic.
* **Cause:** We used **GPIO 2 (D4)** for PWRKEY. This pin is tied to the on-board LED and flashes LOW during boot, triggering the modem's power button.
* **Fix:** Moved PWRKEY wire to **GPIO 18**.

### Issue 3: Unstable Serial
* **Symptom:** Garbage characters or missed data.
* **Fix:** Switched from `SoftwareSerial` to `HardwareSerial(2)` on ESP32.

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
1.  **Wake Up:** Deep Sleep interrupt or Timer.
2.  **Sensor Power:** Turn on Transistor/MOSFET powering sensor (Anti-corrosion measure).
3.  **Read:** Analog Read (A0).
4.  **Send:** Transmit struct via ESP-NOW to Hub MAC Address.
5.  **Sleep:** Enter Deep Sleep immediately.

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
