# 🧊 Project Freezer: Smart Farm Hub (v1.0)

**Status:** STABLE / PRODUCTION READY  
**Date:** January 3, 2026  
**Components:** ESP32 (Hub), EC200U (LTE Modem), Cloud Run (Python), BigQuery, GCS.

---

## 1. ⚠️ CRITICAL LOGIC & "GOTCHAS"
*(Read this before changing any code)*

### A. The "Sticky Header" Bug (Quectel EC200U)
*   **Issue:** The modem's "Raw Header" mode (`AT+QHTTPCFG="requestheader",1`), required for Multipart Image uploads, is **sticky**. It persists across commands.
*   **Symptom:** After an image upload, simple GET requests (Sensor Data) fail immediately with `ERROR` because the modem is still expecting raw headers.
*   **The Fix:** You must explicitly **DISABLE** raw headers (`AT+QHTTPCFG="requestheader",0`) at the start of any function that uses standard GET/POST commands.

### B. The "Dirty State" Reset
*   **Issue:** If an upload fails or times out, the modem often keeps the HTTP context open, blocking future attempts.
*   **The Fix:** A helper function `resetHTTP()` is called before every transaction.
    *   **Command:** `AT+QHTTPSTOP`
    *   **Wait:** 2 seconds.

### C. Cloud Function "Traffic Control"
*   **Logic:** A single Cloud Run endpoint handles two distinct data types using `request.method`.
    *   **POST:** Image Uploads -> Streams to Google Cloud Storage (GCS).
    *   **GET:** Sensor Data -> Inserts into BigQuery.
*   **Critical Python Fix:** The imports use `from datetime import datetime`. Therefore, you must use `datetime.utcnow()` and **NOT** `datetime.datetime.utcnow()`. The latter causes a silent 500 crash.

### D. The "Soda Straw" Delay
*   **Logic:** When piping the image from SPIFFS to the Modem, you cannot dump the file at full speed. The EC200U UART buffer will overflow.
*   **The Fix:** A `delay(20)` is added inside the `while(file.available())` loop.

---

## 2. ⚡ Power Sub-System & Charging Logic

*   **Solar Panel:** Epoxy 12V 4W (Charging at ~85mAh between 11AM - 3PM).
*   **Battery:** 3x 18650 Li-Ion Pack (3S) with BMS.
*   **Charge Controller (CC):** Generic "Blue" PWM Controller.
    *   **Settings:** Mode `b03` (Li-Ion), Cut-off Voltage `12.3V`.
    *   **Safety Update:** A **Diode** is needed between Panel and CC to prevent reverse leakage at night.
*   **BMS "Wake-Up" Protocol:**
    *   **Issue:** BMS trips (shows 3V ghost voltage) if battery is connected to CC first due to capacitor inrush.
    *   **Solution:** **ALWAYS** connect Solar Panel to CC first, *then* connect the Battery. This "jump starts" the BMS.

---

## 3. 🏗 Hardware Architecture (Validated)

*   **Core:** ESP32 (DOIT DevKit V1) + EC200U (4G LTE Modem).
*   **Regulation:** LM2596 Buck Converter (Tuned to **5.1V**).
*   **Sensors:** Capacitive Soil Moisture (Analog).
*   **Board:** 9x15cm Perfboard.
*   **Topology:** "Common Bus" Strategy (Common Grounds).

---

## 4. 🛠 Physical Build Status

*   **Enclosure:** 200x150mm ABS IP65 box.
*   **Soldering:** **PENDING / NEXT STEP**.
*   **Tools:** Bakon BK606A (90W) Iron.
    *   **Tip Selection:** `900M-TK` (Knife) for battery pads, `TB` (Pencil) for ESP32.

### Immediate Next Steps
1.  **Soldering (High Priority):** Transfer the breadboard prototype to the Perfboard.
    *   *Order:* Solder "Power Section" (Screw Terminals + LM2596 Headers) first to verify voltages before adding ESP32.
2.  **Enclosure Prep:** Drill holes in the new 200x150mm box for glands/connectors.
3.  **Final Assembly:** Mount PCB and Battery Pack (with "Seatbelt" tape and "Modem Kiss" insulation).

### Long Term TODO
*   **SMS Alert:** Add feature so Hub sends an SMS immediately upon waking in the morning (Health Check).