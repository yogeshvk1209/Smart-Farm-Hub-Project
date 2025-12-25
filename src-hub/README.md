# FarmHub: LTE Gateway (ESP32 + EC200U)

The **FarmHub** is the central gateway for the distributed IoT system. It listens for incoming sensor packets via **ESP-NOW** (a low-power WiFi protocol) and bridges them to the cloud using a **Quectel EC200U 4G LTE** modem.

## 🧠 Hardware Architecture
* **Controller:** ESP32 Dev Kit V1
* **Modem:** Quectel EC200U (LTE Cat 1)
* **Network:** Jio 4G (India) - *Pure IPv6 Configuration*
* **Cloud Backend:** Google Cloud Run (Serverless) + BigQuery

## 🔌 Pinout & Wiring
The ESP32 communicates with the modem via UART2 (Hardware Serial).

```text
       ESP32                       Modem (EC200U/SIM7600)
    +---------+                   +----------------------+
    |     3V3 |-------------------| VCC (Or 5V/VBUS)     |
    |     GND |-------------------| GND                  |
    | GPIO 16 |<------------------| TXD                  |
    | GPIO 17 |------------------>| RXD                  |
    | GPIO 18 |------------------>| PWRKEY               |
    +---------+                   +----------------------+
```

| ESP32 Pin | EC200U Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **GPIO 16** (RX2) | TXD | UART Receive | Receives data *from* Modem |
| **GPIO 17** (TX2) | RXD | UART Transmit | Sends commands *to* Modem |
| **GPIO 18** | PWRKEY | Power Control | Pull HIGH to boot modem |
| **GND** | GND | Common Ground | **CRITICAL** for reliable UART |
| **VIN / 5V** | VBUS | Power Supply | Modem needs high current bursts |

## 🚀 Key Software Features
### 1. The "Jio-Proof" Connection
The firmware implements a custom AT command sequence to bypass Jio's restrictions on generic IoT devices:
* **Force IPv6:** Manually sets PDP Context to `IPV4V6` to align with Jio's native stack.
* **Roaming Handler:** Includes logic to handle pseudo-roaming states common on IoT SIMs.

### 2. Manual HTTP Engine
Instead of using standard HTTP libraries (which often fail on IPv6-only networks due to header injection issues), this project uses a **Direct AT Command Implementation**:
* Uses `AT+QHTTPURL` and `AT+QHTTPGET` directly.
* Transmits data via **URL Parameters** (GET Request) to ensure 100% delivery success regardless of the modem's internal header formatting.

## 🛠️ Telemetry Flow
1.  **Listen:** ESP32 waits for ESP-NOW packet from a Spoke.
2.  **Parse:** Extracts `device_id`, `moisture_raw`, and `battery`.
3.  **Connect:** Wakes Modem and ensures IPv6 Context is active.
4.  **Upload:** Sends HTTPS GET request to Cloud Run:
    `https://[cloud-run-url]/?device_id=spoke_1&raw=607&pct=69`

## ⚙️ Configuration
Before compiling, you must configure your backend URL:
1.  Go to `src/`.
2.  Rename `secrets_example.h` to `secrets.h`.
3.  Replace the placeholder URL with your deployed Google Cloud Run endpoint.

## 🛠 Debugging & Troubleshooting
For a comprehensive guide on AT commands and debugging the **Quectel EC200U** modem (especially on the Jio network), please refer to the [Modem Debugging Guide](Debug.md).
