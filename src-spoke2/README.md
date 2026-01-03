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
2.  **Capture:** Take a photo (SVGA 800x600, Low Quality/High Comp).
3.  **Stream:** The image is too large for a single packet.
    *   The Spoke splits the JPEG into **240-byte chunks**.
    *   It "blasts" these chunks to the Hub via ESP-NOW.
    *   The Hub buffers them to its internal SPIFFS storage.
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
*   **WIFI_CHANNEL:** Must match the Hub (Default: 1).
*   **Deep Sleep:** 30 minutes (successful cycle), 2 minutes (retry cycle).
