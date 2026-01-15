# FarmHub: Security Spoke (ESP32-CAM)

**Spoke 2** is the security sentinel of the FarmHub system. Unlike the passive Soil Spoke, this node is an "Active Hunter" that wakes up, actively searches for the Hub, and streams high-resolution imagery when a connection is established.

## 🧠 The Logic: "Hunter Protocol"
Since the Spoke has no RTC (Real Time Clock), it cannot know when the Hub is awake. It uses a **"Hunt and Peck"** strategy:

1.  **Wake Up:** The Spoke wakes from deep sleep (approx every 30 mins).
2.  **Ping:** It broadcasts a small "Ping" packet via ESP-NOW to the Hub.
3.  **Listen for ACK:** It listens for a hardware-level Acknowledgement (ACK) from the Hub.
    *   **If ACK Received:** The Hub is awake! Proceed to Camera Sequence.
    *   **If NO ACK:** The Hub is sleeping. The Spoke sleeps for **2 minutes** and retries.
    *   *Result:* The Spoke naturally "drifts" into alignment with the Hub's wake window.

## 📸 Image Transfer Sequence
Once the Hub is confirmed awake:
1.  **Init Camera:** Power up the camera module (OV2640).
    *   **Resolution:** SVGA (800x600)
    *   **Quality:** 10 (High Quality, Low Compression)
2.  **Capture:** Take a photo.
3.  **Stream (Manual Chunking):** The image is too large for a single packet.
    *   **Chunk Size:** The JPEG is split into **240-byte chunks**.
    *   **Retries:** Each chunk attempts to send up to 3 times.
    *   **Critical Delay:** A **40ms delay** is inserted *between every chunk*. This allows the Hub sufficient time to write the previous chunk to its Flash storage (LittleFS) before the next one arrives.
4.  **Sleep:** Mission complete. Sleep for 30 minutes.

## 🔌 Hardware & Pinout

**Board:** ESP32-CAM (AI Thinker)

| ESP32 Pin | Component | Function |
| :--- | :--- | :--- |
| **GPIO 4** | Flashlight | White LED (Optional/PWM) |
| **GPIO 33** | Red LED | Status (Note: Logic is Inverted. LOW = ON) |
| **U0TX/RX** | Serial | Programming & Debug |
| **5V / GND** | Power | Input 5V |

**Camera Pins (OV2640):**
*   **PWDN:** 32, **RESET:** -1
*   **XCLK:** 0, **SIOD:** 26, **SIOC:** 27
*   **D7:** 35, **D6:** 34, **D5:** 39, **D4:** 36
*   **D3:** 21, **D2:** 19, **D1:** 18, **D0:** 5
*   **VSYNC:** 25, **HREF:** 23, **PCLK:** 22

## 📝 Configuration

### Partition Scheme
**CRITICAL:** This project requires the `huge_app` partition scheme to allow sufficient memory for image capture buffer.
*   In `platformio.ini`: `board_build.partitions = huge_app.csv`
*   Build Flags: `-DBOARD_HAS_PSRAM`

### Settings
*   **WIFI_CHANNEL:** Must match the Hub (Default: 1).
*   **Deep Sleep:** 30 minutes (successful cycle), 2 minutes (retry cycle).

## 🚧 Technical Awareness
1.  **Critical Timing (Magic Numbers):** The `delay(40)` between chunks is a tuned value. If the Hub's write speed changes (e.g., due to file system fragmentation or a different flash chip), this value might need adjustment to prevent packet loss.
2.  **Hardcoded MAC:** The Hub's MAC address is currently hardcoded in `main.cpp` and must be manually updated for each deployment.
