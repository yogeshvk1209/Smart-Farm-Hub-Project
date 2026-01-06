# 🧊 Project Freezer: Smart Farm Hub (v1.2)

**Status:** 🟡 TESTING (Roof Bake-in Phase)
**Date:** January 06, 2026
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

## 2. ⚡ Power Sub-System & Charging Logic

* **Solar Panel:** Epoxy 12V 4W (Charging at ~85mAh between 11AM - 3PM).
* **Battery:** 3x 18650 Li-Ion Pack (3S) with BMS.
* **The "Power Tower" (Physical Stack):**
    * **Charge Controller:** Generic "Blue" PWM Controller (Mode `b03` for Li-Ion, Cut-off `12.3V`).
    * **Regulation:** LM2596 Buck Converter (Step down to **5.1V** for ESP32/Modem).
    * **Protection:** Diode installed between Panel and CC to prevent reverse night leakage.
* **BMS "Wake-Up" Protocol:**
    * **Rule:** **ALWAYS** connect Solar Panel to CC first, *then* connect the Battery. This prevents the BMS from tripping (showing ghost voltage) due to capacitor inrush.

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
    * *Status:* Transfer from breadboard to Perfboard successful.
    * *Verification:* "Power Tower" assembled and voltage levels verified. Image transmission and Sensor pulse confirmed post-solder.
* **Current State:** 🟡 **ROOF BAKE-IN (48-72 Hours)**.
    * *Location:* Rooftop (Full sun/weather exposure).
    * *Objective:* Verify thermal stability and power cycling under real-world conditions.

### Bake-In Success Criteria (Pass/Fail)
1.  **Reliability:** Zero "Hard Resets" or Brownouts over 72 hours.
2.  **Logic:** Correctly wakes up at scheduled intervals (07:00, 12:00, 17:00).
3.  **Data:** BigQuery receives data packets consistently without gaps.
4.  **Thermals:** Components (Regulator/Modem) remain safe inside the sealed box during peak noon sun.

### Immediate Next Steps
1.  **Analyze Logs:** Review BigQuery/GCS after 72 hours for any missed heartbeats.
2.  **Final Sealing:** Apply Silicone to cable glands and close lid screws permanently.
3.  **Deployment:** Move from Roof Test to Field Pole.

### Long Term TODO
* **SMS Alert:** Add feature so Hub sends an SMS immediately upon waking in the morning (Health Check).
* **Variablise the Start-End time: ** Add feature to allow for Start/End time to be update from SMS or other options
