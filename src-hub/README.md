# FarmHub: LTE Gateway (ESP32)

The **FarmHub** is the central gateway for the distributed IoT system. It listens for incoming sensor packets via **ESP-NOW**, aggregates the data, and uploads it to Google Cloud via **4G LTE**.

> **✅ Current Status:** The code is **Production Ready**. It features robust **LTE Connectivity (Jio)**, **RTC Sleep Synchronization**, **Image Upload Capability**, and **ESP-NOW Reception**.

## 🧠 Hardware Architecture (VALIDATED)
*   **Controller:** ESP32 (DOIT DevKit V1)
*   **Modem:** Quectel EC200U (4G LTE) via UART.
*   **Timekeeping:** DS3231 RTC (I2C) for precision wake-ups.
*   **Storage:** **LittleFS** (replacing SPIFFS) used for buffering large Camera images from RAM to Flash before upload.
*   **Power:** 
    *   **Source:** 3x 18650 Li-Ion Pack (3S) with BMS.
    *   **Charging:** 20W Solar Panel -> LM2596 (13.9V) -> Diode (IN5408S) -> BMS.
    *   **Regulation:** LM2596 Buck Converter (Tuned to 5.1V) for System Load.
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
    |         |                   |                      |
    | GPIO 34 |<------------------| Battery Voltage Div  |
    +---------+                   +----------------------+
```

## 🚀 Key Software Features

### 1. Robust LTE Connectivity & Bug Fixes
The Modem logic has been hardened to handle "real world" cellular quirks:
*   **Sticky Header Fix:** Explicitly disables "Raw Header" mode (`AT+QHTTPCFG="requestheader",0`) before standard GET requests to prevent the modem from confusing sensor data with image uploads.
*   **Dirty State Reset:** The `resetHTTP()` function runs `AT+QHTTPSTOP` before every transaction to clear any hung contexts.
*   **Jio Specifics:** Dedicated connection sequence for `jionet` APN.

### 2. Image Ingestion (RAM -> LittleFS -> Cloud)
The Hub acts as a bridge for the Camera Spoke with a specific high-reliability flow:
1.  **Ingest:** Incoming ESP-NOW image chunks are buffered in **RAM** (`imgBuffer`, ~50KB allocated).
2.  **Save:** Once the stream ends (timeout), the buffer is written to **LittleFS** (`/cam_capture.jpg`).
3.  **Upload:** The file is then read from LittleFS and streamed to the Modem.
4.  **"Soda Straw" Logic:** A `delay(20)` is inserted inside the modem stream loop to prevent overflowing the UART buffer.

### 3. "Waiting" Heartbeat
When active but not receiving data, the Hub prints a `(Waiting for Camera Data...)` heartbeat every 5 seconds. This visual cue confirms the device is not frozen while waiting for the "Hunter" Spoke to connect.

### 4. "Early Riser" Sleep Logic
The Hub manages a complex sleep schedule to minimize power while ensuring it catches all Spoke transmissions.
*   **Night Mode:** Hibernates from **19:00 to 06:58**.
*   **Day Mode:** Wakes up for **4-minute windows** centered around packet transmission times.
    *   **Windows:** :58, :13, :28, :43.
*   **Nap Mode:** If the window closes, it calculates the *exact* seconds until the next window and deep sleeps.

### 5. ESP-NOW Receiver
*   Configured on **WiFi Channel 1**.
*   Promiscuous mode enabled briefly to force channel selection.
*   Registers a callback `OnDataRecv` to handle incoming structures.

## 🛠️ Telemetry Flow
1.  **Wake Up:** Hub wakes -> Checks RTC.
2.  **Modem Init:** Powers on Modem -> Waits for Signal -> Connects to `jionet`.
3.  **Listen:** enters Active Window.
4.  **Receive:** Packet arrives from Spoke -> Parsed.
5.  **Upload:** Immediately performs an HTTP GET/POST to Google Cloud Run.
6.  **Sleep:** When the window ends, the Hub calculates the next wake-up time and sleeps.

## ⚙️ Configuration
Hardcoded in `src/main.cpp`:
```cpp
const String gcp_url = "https://ingest-farm-data-....run.app";
const int WINDOW_DURATION = 4; 
const int WIFI_CHANNEL = 1;    
#define SPIFFS LittleFS // Using LittleFS filesystem
```

**Security:**
The project now uses a `secrets.h` file (excluded from git) to store sensitive keys.
*   Create `src/secrets.h` based on `src/secrets_example.h`.
*   Define: `#define API_KEY "YOUR_SECRET_KEY"`

## 🛠️ Utilities & Debugging
### GSM Modem Passthrough (`gsm_testing_main.cpp`)
A standalone sketch is provided to debug modem issues directly.
1.  Rename `src/main.cpp` to `src/main_hub.cpp`.
2.  Rename `gsm_testing_main.cpp` to `src/main.cpp`.
3.  Upload to ESP32.
4.  Open Serial Monitor at 115200.
5.  Type AT commands (e.g., `AT`, `AT+CSQ`, `AT+CREG?`) to talk directly to the modem.

## 🚧 Future Improvements & Technical Awareness

### Feature Roadmap
1.  **OTA Updates:** Implement Over-The-Air firmware updates via LTE.
2.  **Two-Way Comms:** Allow the Hub to send configuration back to Spokes (e.g., change wake intervals).

### Codebase Awareness (Technical Debt)
1.  **Memory Management:** The `imgBuffer` is allocated via `malloc` but never freed. While acceptable for this dedicated-purpose firmware (which relies on Deep Sleep resets), it is a technical deviation from standard C++ practices.
2.  **Blocking Operations:** The Modem interaction uses `modem.waitResponse()` which is blocking. This pauses the main loop during uploads, preventing the Hub from processing other sensor packets if they arrive simultaneously during an upload.
3.  **Hardcoded Configuration:** MAC addresses and timing constants are currently hardcoded in `main.cpp`. These should ideally be moved to `secrets.h` or a dedicated configuration header.