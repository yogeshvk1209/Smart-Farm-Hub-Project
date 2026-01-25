❄️ Project Freezer: South Karnataka Farm Hub (V1.0)
Date: January 25, 2026

Status: Production Stable (Verified Telemetry + Verified Binary JPEG)

📌 Project Overview
A "Hub and Spoke" farm automation system designed for semi-arid environments. The system uses an ESP32 + EC200U LTE Hub to collect soil moisture telemetry and camera images from distributed ESP-NOW Spokes, then pushes the data to Google Cloud Platform (GCP) via a Python-based Cloud Run backend.

🛠 Hardware Stack
Central Hub: ESP32 (MCU) + Quectel EC200U (LTE Modem).

Power: 20W Solar Panel + Battery Management.

Sensing: Soil Moisture (Capacitive) + Battery Voltage monitoring (GPIO 34).

Communication: * Local: ESP-NOW (Long Range Mode).

Cloud: 4G LTE (Jio APN: jionet) via AT Commands.

⚡ Key Challenges & Engineering Fixes
1. The "Data vs JPEG" Binary Corruption
Issue: Images arrived at GCS as generic data files that wouldn't open. Terminal head checks revealed "blank gaps" in the bytes.

Root Cause: UART buffer overrun. The ESP32 was pushing binary data at 115200 bps faster than the modem could transmit over the 4G network.

Fix: * Dynamic Baud Switching: The Hub now drops to 57600 bps strictly for image uploads.

Trickle-Feed: Data is sent in small 128-byte chunks with a 45ms delay to ensure the modem's internal buffer never overflows.

Header Alignment: A "Header Hunt" logic was added to find the 0xFF 0xD8 marker, ensuring every file starts exactly at the JPEG SOI.

2. Mid-Air Collisions (The Mutex Lock)
Issue: Telemetry uploads would fail or "Square 1" errors would occur if a sensor packet arrived while an image was being uploaded.

Fix: Implemented a Global Mutex Lock (isModemBusy). The Hub now serializes tasks—finishing the image completely before allowing the modem to switch back to telemetry mode.

3. GPRS Context Deadlocks
Issue: The modem would "hang" or go silent if an AT command didn't receive the expected CONNECT or OK prompt.

Fix: Replaced raw serial.find() with Library-managed Timeouts and guaranteed "Unlock" logic. Even if a transmission fails, the Hub resets its state and remains ready for the next cycle.

4. Ghost/Noise Uploads
Issue: Frequent 1-byte file uploads triggered by noise or handshake packets.

Fix: A Minimum Size Filter (1000 bytes) ensures the Hub only initiates a heavy 4G POST request if a substantial amount of image data has actually been collected.

📊 Cloud Integration Details
BigQuery: Stores moisture (%) and battery voltage (V).

Note: Strict type-casting (Int/Float) was added to the Python backend to prevent silent ingestion failures.

GCS: Stores JPEGs in a date-partitioned structure: uploads/YYYY/MM/DD/spoke_X_timestamp.jpg.

Security: Token-based authentication (FARM_SEC) is enforced on all incoming HTTP requests.

🚜 Maintenance Notes for South Karnataka Deployment
Calibration: If battery readings drift, adjust the VOLT_FACTOR (currently 11.24) in the Hub firmware.

Signal: Ensure the EC200U antenna is clear of metal obstructions. The system is currently optimized for Jio's 4G latency.
