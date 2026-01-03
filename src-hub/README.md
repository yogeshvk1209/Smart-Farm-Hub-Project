# FarmHub: LTE Gateway (ESP32)

The **FarmHub** is the central gateway for the distributed IoT system. It listens for incoming sensor packets via **ESP-NOW**, aggregates the data, and uploads it to Google Cloud via **4G LTE**.

> **✅ Current Status:** The code is **Production Ready**. It features robust **LTE Connectivity (Jio)**, **RTC Sleep Synchronization**, **Image Upload Capability**, and **ESP-NOW Reception**.

## 🧠 Hardware Architecture (VALIDATED)
*   **Controller:** ESP32 (DOIT DevKit V1)
*   **Modem:** Quectel EC200U (4G LTE) via UART.
*   **Timekeeping:** DS3231 RTC (I2C) for precision wake-ups.
*   **Storage:** Internal SPIFFS (used for buffering Camera images).
*   **Power:** 
    *   **Source:** 3x 18650 Li-Ion Pack (3S) with BMS.
    *   **Regulation:** LM2596 Buck Converter (Tuned to 5.1V).
    *   **Solar:** 12V 4W Panel + PWM Charge Controller.
*   **Board:** 9x15cm Perfboard with Common Bus topology.

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

### 1. Robust LTE Connectivity & Bug Fixes
The Modem logic has been hardened to handle "real world" cellular quirks:
*   **Sticky Header Fix:** Explicitly disables "Raw Header" mode (`AT+QHTTPCFG="requestheader",0`) before standard GET requests to prevent the modem from confusing sensor data with image uploads.
*   **Dirty State Reset:** The `resetHTTP()` function runs `AT+QHTTPSTOP` before every transaction to clear any hung contexts.
*   **Jio Specifics:** Dedicated connection sequence for `jionet` APN.

### 2. Image Ingestion & The "Soda Straw"
The Hub acts as a bridge for the Camera Spoke:
*   **Buffering:** Incoming ESP-NOW image chunks are written to a file in SPIFFS (`/cam_capture.jpg`).
*   **Streaming:** The file is then streamed to the Modem.
*   **"Soda Straw" Logic:** A critical `delay(20)` is inserted inside the stream loop. This prevents the ESP32 from overwhelming the Modem's UART buffer (The "Soda Straw" effect).

### 3. "Early Riser" Sleep Logic
The Hub manages a complex sleep schedule to minimize power while ensuring it catches all Spoke transmissions.
*   **Night Mode:** Hibernates from **19:00 to 07:00**.
*   **Day Mode:** Wakes up for 5-minute windows centered around packet transmission times.
*   **Nap Mode:** If the window closes, it calculates the *exact* seconds until the next window (:28 or :58) and deep sleeps.

### 4. ESP-NOW Receiver
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
