Project: Farm Sentinel (V1.0)Goal: Solar-powered farm monitoring (Soil + Security) with Cellular Upload.1. System ArchitectureNetwork Protocol: ESP-NOW (Private Wi-Fi, Mac-to-Mac).Topology: Star Topology (Multiple Spokes -> One Central Hub).Uplink: 4G LTE (Jio) via SIM7600/EC200U Modem.Cloud: Google Cloud Functions (Python) + BigQuery + Cloud Storage.2. Hardware & Pin MapsA. Spoke 1: Soil Monitor
Hardware: ESP8266 (NodeMCU) + Capacitive Soil Sensor.
Power: Li-ion Battery (Sleeps 99% of time).
Logic: Wakes every 30 mins -> Reads Analog -> Sends Struct -> Sleeps.

B. Spoke 2: Security Camera
Hardware: ESP32-CAM (AI Thinker).
Power: Li-ion Battery (Hunter Mode).
Logic:
Wakes up (Light sleep).
Hunter Mode: Pings Hub every 1s for up to 60s.
Sync: If ACK received -> Init Camera -> Snap -> Send Chunks -> Sleep 30m.
Fail: If no ACK -> Sleep 2m -> Retry.

ESP32-CAM Pin   Component
GPIO 4          Flashlight (Optional)
GPIO 33         Red LED (Inverted)
U0TX/RX         Programming/Debug
3.3V/5V         Power Input

C. The Hub: The Brain
Hardware: ESP32 (DevKit) + EC200U (or SIM7600) + DS3231 RTC.
Storage: SPIFFS (Internal Flash) for image buffering.
Schedule: Wakes 4 times/hour (:58, :13, :28, :43) for 4 minutes.

Hub Pin     Component               Function
GPIO 16     Modem TX                RX (Serial2)
GPIO 17     Modem RX                TX (Serial2)
GPIO 18     Modem PWR               Power Key (Pulse to wake)
GPIO 34     Voltage Div             Battery Monitor (Analog)
SDA (21)    DS3231 RTC              I2C Data
SCL (22)    DS3231 RTC              I2C Clock

## 4. Updates (Jan 6, 2026)
*   **Documentation Alignment:**
    *   Updated `src-hub/README.md` to reflect the 4-window sleep schedule and 4-minute duration.
    *   Added Battery Voltage Divider (GPIO 34) to Hub Pinout in README and `perfBoardPlan.md`.
    *   Corrected Spoke 1 Hardware (ESP8266) in status docs.
*   **Code Analysis:**
    *   **Hub:** Robust Modem logic. **Caution:** File I/O in ESP-NOW callback (`OnDataRecv`) is high-risk for blocking.
    *   **Spoke 1:** Efficient deep sleep logic. **Note:** Battery voltage currently sending placeholder 0.0. Soil sensor uses single-sample read (consider averaging).
3. Software Logic (The "Secret Sauce")The Synchronization Hack (Hunter Protocol)Since the Spoke has no RTC, it cannot know when the Hub is awake.Hub wakes at fixed times (xx:28) based on its high-precision DS3231.Spoke uses a "Hunt and Peck" strategy:It wakes up and "pings" the Hub.If Hub is Asleep: No ACK. Spoke sleeps for 2 minutes and tries again.If Hub is Awake: Hardware ACK received. Spoke fires data immediately.Result: The Spoke naturally "drifts" into alignment with the Hub's window.The Image Transfer (Chunking)ESP-NOW cannot send 15KB images in one go (Max 250 bytes).Spoke: Splits JPEG into 240-byte chunks and blasts them.Hub:Detects len > 50 = "This is a Camera Chunk".Opens /cam_capture.jpg in SPIFFS.Appends data until silence > 2 seconds.Upload: Hub reads file from SPIFFS and uses Multipart POST to GCP.4. Deployment Checklist (The "Go Live" Steps)Final Code Cleanup:Hub: Open main.cpp. Search for manageSleep. DELETE return;.Hub: ensure WIFI_CHANNEL = 1.Spoke: ensure hubMacAddress matches the Hub.Power Sequence:Turn on Hub first. Let it stabilize (it might sleep immediately, that's fine).Turn on Spoke.Physical Install:Antennas: Ensure the Modem (LTE) and WiFi antennas are perpendicular if possible.Waterproofing: Use silica gel packets inside your box. Condensation kills ESP32-CAMs.Verification:Wait 30-60 minutes.Check Google Cloud Storage.Note: Do not panic if you miss the first cycle. The "Hunter" algorithm might take 1-2 cycles to sync up perfectly.
