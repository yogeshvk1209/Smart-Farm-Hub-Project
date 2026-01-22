# Smart Farm Hub Project

**Status:** STABLE / PRODUCTION READY  
**Last Updated:** Jan 22, 2026

## 📖 Project Overview
A decentralized wireless sensor network for farm monitoring. The system consists of low-power "Spoke" nodes (sensors & cameras) communicating via **ESP-NOW** to a central "Hub" gateway, which uploads data to Google Cloud via **4G LTE**.

## 🏗 System Architecture (Star Topology)

### 1. The Hub (Central Gateway)
*   **Role:** The Brain. Wakes up on a schedule to receive data from Spokes.
*   **Hardware:** ESP32 + Quectel EC200U (4G LTE) + DS3231 RTC.
*   **Connectivity:** ESP-NOW (Local) & LTE (Cloud).
*   **Power:** Solar-charged 3S Li-Ion Battery Pack.
*   **Key Feature:** Uses a "Soda Straw" technique to stream large images to the modem without overflowing buffers.

### 2. Spoke 1 (Soil Monitor)
*   **Role:** The Observer. Monitors soil moisture levels.
*   **Hardware:** ESP8266/ESP32 + Capacitive Soil Sensor.
*   **Logic:** Wakes every 30 mins -> Reads Analog -> Sends Data -> Sleeps.
*   **Power:** Ultra-low power sleep (Year-long battery life).

### 3. Spoke 2 (Security Camera)
*   **Role:** The Hunter. Monitors for security threats.
*   **Hardware:** ESP32-CAM (AI Thinker).
*   **Logic:** **"Hunter Protocol"**
    *   Wakes up and "hunts" for the Hub by sending Ping packets.
    *   If Hub responds (ACK), it snaps a photo and streams it in chunks.
    *   If Hub is silent, it naps for 2 mins and retries.

---

## 💻 Software & Cloud

### Backend (Google Cloud)
*   **Ingest:** Python Cloud Function (Unified Endpoint).
    *   **GET Requests:** Telemetry data -> BigQuery.
    *   **POST Requests:** Image streams -> Google Cloud Storage (GCS).
*   **Data Warehouse:** BigQuery (SQL Analysis).

### Firmware Highlights
*   **Sticky Header Fix:** Hub intelligently toggles modem "Raw Header" modes to switch between JSON uploads and Multipart Image streams.
*   **Sync:** Uses DS3231 RTC on Hub to maintain a precise "Wake Window". Spokes drift into this window using their specific retry logic.

---

## 🛠 Getting Started

### Prerequisites
*   **VS Code** with **PlatformIO** extension.
*   **Google Cloud Account**.
*   **Hardware:** ESP32s, ESP32-CAM, EC200U/SIM7600 Modem, 12V Solar Setup.

### Repository Structure
*   `src-hub/`: Code for the Central LTE Gateway.
*   `src-spoke1/`: Code for the Soil Moisture Sensor.
*   `src-spoke2/`: Code for the Security Camera (ESP32-CAM).
*   `backend/`: Python Cloud Functions for GCP.

---

## 🚀 Status
*   [x] **Hardware:** Validated (Perfboard + Li-Ion 3S).
*   [x] **Connectivity:** 4G LTE (Jio) Stable.
*   [x] **Cloud:** Pipeline Live (Cloud Run -> BigQuery/GCS).
*   [x] **Spoke 1:** Soil Sensing Active.
*   [x] **Spoke 2:** Camera Streaming Active.

## 📝 Roadmap & TODOs
### Spoke 1 (Soil Node)
- [ ] **Signal Averaging:** Improve accuracy by replacing the single `analogRead` with a multi-sample average (e.g., 10 readings).
- [ ] **Telemetry:** Implement Battery Voltage monitoring. Read the voltage divider on the analog pin and report it in the `struct_message` (Currently sending 0.0).

### The Hub
- [ ] **Daily Health SMS:** Implement a "Morning Roll Call". When the Hub wakes at **07:00 AM**, it should send an SMS to the admin containing:
    -   Hub Battery Voltage.
    -   LTE Signal Strength (CSQ).
    -   Summary of received packets/images from previous day.
