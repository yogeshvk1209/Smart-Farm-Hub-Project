# 🧊 Project Freezer: Smart Farm Hub (v1.2)

**Status:** 🟢 FIELD TESTING (Production Candidate)
**Date:** January 22, 2026
**Components:** ESP32 (Hub), EC200U (LTE Modem), Cloud Run (Python), BigQuery, GCS.

---

## 1. ⚠️ CRITICAL LOGIC & "GOTCHAS"
*(Read this before changing any code)*

### A. The "Sticky Header" Bug (Quectel EC200U)
* **Issue:** The modem's "Raw Header" mode (`AT+QHTTPCFG="requestheader",1`), required for Multipart Image uploads, is **sticky**. It persists across commands.
* **Symptom:** After an image upload, simple GET requests (Sensor Data) fail immediately with `ERROR` because the modem is still expecting raw headers.
* **The Fix:** You must explicitly **DISABLE** raw headers (`AT+QHTTPCFG="requestheader",0`) at the start of any function that uses standard GET/POST commands.

### B. The "Dirty State" Reset
* **Issue:** If an upload fails or times out, the modem often keeps the HTTP context open, blocking future attempts.
* **The Fix:** A helper function `resetHTTP()` is called before every transaction.
    * **Command:** `AT+QHTTPSTOP`
    * **Wait:** 2 seconds.

### C. Cloud Function "Traffic Control"
* **Logic:** A single Cloud Run endpoint handles two distinct data types using `request.method`.
    * **POST:** Image Uploads -> Streams to Google Cloud Storage (GCS).
    * **GET:** Sensor Data -> Inserts into BigQuery.
* **Critical Python Fix:** The imports use `from datetime import datetime`. Therefore, you must use `datetime.utcnow()` and **NOT** `datetime.datetime.utcnow()`. The latter causes a silent 500 crash.

### D. The "Soda Straw" Delay
* **Logic:** When piping the image from SPIFFS to the Modem, you cannot dump the file at full speed. The EC200U UART buffer will overflow.
* **The Fix:** A `delay(20)` is added inside the `while(file.available())` loop.

---

## 2. ⚡ Power Sub-System (Field Config)

*   **Solar Panel:** 20W (Oversized for safety).
*   **Charging Chain:** `Panel` $\rightarrow$ `Buck (13.9V)` $\rightarrow$ `Diode` $\rightarrow$ `BMS` $\rightarrow$ `3S Li-Ion`.
*   **System Power:** `Battery` $\rightarrow$ `Buck (5.1V)` $\rightarrow$ `ESP32/Modem`.
*   **⚠️ IMPORTANT:** See `recommended.md` for a critical safety fix regarding the charging voltage.

---

## 3. 🏗 Hardware Architecture (Validated)

* **Core:** ESP32 (DOIT DevKit V1) + EC200U (4G LTE Modem).
* **Sensors:** Capacitive Soil Moisture (Analog).
* **Board:** 9x15cm Perfboard (Soldered).
* **Topology:** "Common Bus" Strategy (Star Grounding to reduce noise).

---

## 4. 🛠 Physical Build Status

* **Enclosure:** 200x150mm ABS IP65 box.
* **Soldering:** ✅ **COMPLETED**.
* **Current State:** 🟢 **FIELD DEPLOYED**.
    * *Objective:* Long-term reliability test.
    * *Monitor:* Battery voltage telemetry in BigQuery.

### Success Criteria
1.  **Reliability:** Zero "Hard Resets" or Brownouts.
2.  **Logic:** Correctly wakes up at scheduled intervals.
3.  **Data:** BigQuery receives data packets consistently.

### Long Term TODO
* **SMS Alert:** Add feature so Hub sends an SMS immediately upon waking in the morning (Health Check).
* **Variablise the Start-End time:** Add feature to allow for Start/End time to be update from SMS.

