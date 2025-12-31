# FarmHub: LTE Gateway (ESP32)

The **FarmHub** is the central gateway for the distributed IoT system. It listens for incoming sensor packets via **ESP-NOW** and coordinates sleep schedules with the Spokes.

> **⚠️ Current Status:** The code currently implements the **RTC Sleep Synchronization** and **ESP-NOW Reception**. The LTE Modem integration (AT Commands) is currently pending/commented out in `main.cpp`.

## 🧠 Hardware Architecture
* **Controller:** ESP32 Dev Kit V1
* **Timekeeping:** DS3231 RTC (I2C)
* **Modem:** Quectel EC200U (LTE Cat 1) - *Integration Pending*
* **Network:** ESP-NOW (Receiver Role)

## 🔌 Pinout & Wiring

```text
       ESP32                       Peripherals
    +---------+                   +----------------------+
    |     3V3 |-------------------| RTC VCC              |
    |     GND |-------------------| RTC GND              |
    | GPIO 21 |-------------------| RTC SDA              |
    | GPIO 22 |-------------------| RTC SCL              |
    |         |                   |                      |
    | GPIO 16 |<------------------| Modem TXD (Pending)  |
    | GPIO 17 |------------------>| Modem RXD (Pending)  |
    +---------+                   +----------------------+
```

## 🚀 Key Software Features
### 1. RTC-Synchronized Sleep Windows
To save power, the Hub does not stay awake continuously. Instead, it wakes up during specific "Active Windows" to listen for Spoke data.
*   **Window Duration:** 5 Minutes.
*   **Active Times:** 
    *   `XX:29` to `XX:34`
    *   `XX:59` to `XX:04`
*   **Logic:** If the Hub wakes up outside these windows, it calculates the exact seconds remaining until the next window and enters Deep Sleep immediately.

### 2. ESP-NOW Receiver
*   Configured on **WiFi Channel 1**.
*   Promiscuous mode enabled briefly to force channel selection.
*   Registers a callback `OnDataRecv` to handle incoming structures.

## 🛠️ Telemetry Flow (Current)
1.  **Wake Up:** Hub wakes from Deep Sleep or Power On.
2.  **Check Time:** Reads DS3231 RTC.
3.  **Window Check:** 
    *   If inside Window (e.g. 10:30): Stay awake, init ESP-NOW, listen.
    *   If outside Window (e.g. 10:15): Calculate sleep time (14 mins), Sleep.
4.  **Receive:** Packet arrives from Spoke -> Parsed into `struct_message`.
5.  **Loop:** Continues listening until the window closes (monitored in `loop()`).

## ⚙️ Configuration
The configuration is hardcoded in `src/main.cpp`:
```cpp
const int WINDOW_DURATION = 5; // Stay awake for 5 minutes
const int WIFI_CHANNEL = 1;    // MUST match Spoke
```

## 🚧 Planned Features (To Do)
1.  **Modem Wake-up:** Re-enable GPIO 18/17/16 logic.
2.  **Data Upload:** Implement the `AT+QHTTPGET` sequence to upload received data to Google Cloud.
