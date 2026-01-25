# FarmHub: LTE Gateway (ESP32)

The **FarmHub** is the central gateway for the distributed IoT system. It listens for incoming sensor packets via **ESP-NOW**, aggregates the data, and uploads it to Google Cloud via **4G LTE**.

> **✅ Current Status:** The code is **Production Ready (V5.0.0)**. It features robust **LTE Connectivity (Jio)**, **Deadlock-Free Mutex Logic**, **Reliable Image Uploads**, and **ESP-NOW Reception**.

## 🧠 Hardware Architecture (VALIDATED)
*   **Controller:** ESP32 (DOIT DevKit V1)
*   **Modem:** Quectel EC200U (4G LTE) via UART.
*   **Timekeeping:** DS3231 RTC (I2C) (Available but currently not triggering Deep Sleep in V5.0.0).
*   **Storage:** **RAM Buffer** (Allocated on Heap) used for buffering Camera images before upload. *LittleFS removed for performance.*
*   **Power:** 
    *   **Source:** 3x 18650 Li-Ion Pack (3S) with BMS.
    *   **Charging:** **20W Solar Panel** -> Buck Converter -> BMS.
    *   **Regulation:** Buck Converter (Tuned to 5.1V) for System Load.
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

## 🚀 Key Software Features (V5.0.0)

### 1. Robust LTE Connectivity & Deadlock Prevention
The Modem logic has been hardened to handle "real world" cellular quirks:
*   **Global Mutex (`isModemBusy`):** Serializes Telemetry and Image uploads to prevent UART collisions.
*   **Dynamic Baud Rate:** Switches Modem to **57600 bps** for stable image uploads, then restores **115200 bps** for telemetry.
*   **Jio Specifics:** Dedicated connection sequence (`+QICSGP=1,3,"jionet"`) for proper GPRS context.

### 2. "Trickle-Feed" Image Upload
The Hub uses a specialized flow to prevent UART buffer overruns:
1.  **Ingest:** Incoming ESP-NOW image chunks are buffered in **RAM** (`imgBuffer`).
2.  **Header Hunt:** Scans the buffer for the JPEG Start-Of-Image marker (`0xFF 0xD8`) to align the data stream.
3.  **Stream:** Pushes data to the modem in small **128-byte chunks** with a **45ms delay**.
4.  **Verify:** Waits for HTTP 200 OK before clearing the buffer.

### 3. Noise Filtering
*   **Minimum Size Gate:** Discards "Ghost" uploads smaller than 1000 bytes.
*   **Timeout:** Clears the buffer if an image is incomplete after 4 seconds to prevents memory leaks.

### 4. ESP-NOW Receiver
*   Configured on **WiFi Channel 1** (as per Modem interference testing).
*   Registers a callback `OnDataRecv` to handle incoming structures.

## 🛠️ Telemetry Flow
1.  **Start:** Hub initializes Modem & ESP-NOW.
2.  **Listen:** Continuously monitors for incoming ESP-NOW packets.
3.  **Process:** 
    *   If **Telemetry**: Reads Battery Voltage -> Uploads to BigQuery via GET.
    *   If **Image**: Buffers to RAM -> "Trickle" Uploads to GCS via POST.
4.  **Repeat:** System remains active (Always-On).

## ⚙️ Configuration
Hardcoded in `src/main.cpp`:
```cpp
#define SECRETS_GCP_URL "http://YOUR_CLOUD_RUN_URL" // From secrets.h
#define MODEM_PWRKEY 18 
const int MAX_IMG_SIZE = 60000; 
```

**Security:**
The project now uses a `secrets.h` file (excluded from git) to store sensitive keys.
*   Create `src/secrets.h` based on `src/secrets_example.h`.
*   Define: `#define SECRETS_GCP_URL "http://..."`

## 🛠️ Utilities & Debugging
### GSM Modem Passthrough (`gsm_testing_main.cpp`)
A standalone sketch is provided to debug modem issues directly.
1.  Rename `src/main.cpp` to `src/main_hub.cpp`.
2.  Rename `gsm_testing_main.cpp` to `src/main.cpp`.
3.  Upload to ESP32.
4.  Open Serial Monitor at 115200.
5.  Type AT commands (e.g., `AT`, `AT+CSQ`) to talk directly to the modem.
