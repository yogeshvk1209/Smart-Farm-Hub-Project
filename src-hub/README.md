# FarmHub: LTE Gateway (ESP32)

The **FarmHub** is the central gateway for the distributed IoT system. It listens for incoming sensor packets via **ESP-NOW**, aggregates the data, and uploads it to Google Cloud via **4G LTE**.

> **✅ Current Status:** The code is **Production Ready**. It features robust **LTE Connectivity (Jio)**, **RTC Sleep Synchronization**, and **ESP-NOW Reception**.

## 🧠 Hardware Architecture
* **Controller:** ESP32 Dev Kit V1
* **Timekeeping:** DS3231 RTC (I2C)
* **Modem:** SIM7600 / Quectel EC200U (configured for generic AT commands via TinyGSM)
* **Network:** 
    *   **Local:** ESP-NOW (Receiver Role)
    *   **Cloud:** 4G LTE (HTTP GET)

## 🔌 Pinout & Wiring

```text
       ESP32                       Peripherals
    +---------+                   +----------------------+
    |     3V3 |-------------------| RTC VCC              |
    |     GND |-------------------| RTC GND              |
    | GPIO 21 |-------------------| RTC SDA              |
    | GPIO 22 |-------------------| RTC SCL              |
    |         |                   |                      |
    | GPIO 16 |<------------------| Modem TXD            |
    | GPIO 17 |------------------>| Modem RXD            |
    | GPIO 18 |------------------>| Modem PWRKEY         |
    +---------+                   +----------------------+
```

## 🚀 Key Software Features
### 1. Robust LTE Connectivity
*   **Jio Specifics:** The code includes a dedicated `connectToJio()` sequence that handles context activation (`+QICSGP`, `+QIACT`) specifically for the `jionet` APN.
*   **Signal Wait:** Implements a critical wait loop (`waitForNetwork`) to ensure the modem has tower signal before attempting data context.
*   **Security:** Appends a secret token (`FARM_SECRET_2026`) to all requests.

### 2. "Early Riser" Sleep Logic
The Hub manages a complex sleep schedule to minimize power while ensuring it catches all Spoke transmissions.
*   **Night Mode:** Hibernates from **19:00 to 07:00**.
*   **Day Mode:** Wakes up for 5-minute windows centered around packet transmission times.
*   **Nap Mode:** If the window closes, it calculates the *exact* seconds until the next window (:28 or :58) and deep sleeps.

### 3. ESP-NOW Receiver
*   Configured on **WiFi Channel 1**.
*   Promiscuous mode enabled briefly to force channel selection.
*   Registers a callback `OnDataRecv` to handle incoming structures.

## 🛠️ Telemetry Flow
1.  **Wake Up:** Hub wakes -> Checks RTC.
2.  **Modem Init:** Powers on Modem -> Waits for Signal -> Connects to `jionet`.
3.  **Listen:** enters Active Window.
4.  **Receive:** Packet arrives from Spoke -> Parsed.
5.  **Upload:** Immediately performs an HTTP GET to Google Cloud Run.
    *   `GET /?device_id=1&raw=...&pct=...&token=...`
6.  **Sleep:** When the window ends, the Hub calculates the next wake-up time and sleeps.

## ⚙️ Configuration
Hardcoded in `src/main.cpp`:
```cpp
const String gcp_url = "https://ingest-farm-data-....run.app";
const String API_KEY = "FARM_SECRET_2026";
const int WINDOW_DURATION = 5; 
const int WIFI_CHANNEL = 1;    
```

## 🚧 Future Improvements
1.  **OTA Updates:** Implement Over-The-Air firmware updates via LTE.
2.  **Two-Way Comms:** Allow the Hub to send configuration back to Spokes (e.g., change wake intervals).
