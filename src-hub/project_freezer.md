🧊 PROJECT STATE: SMART FARM HUB (Status: ACTIVE - Hardware Assembly)
Last Updated: Jan 2026

1. The Goal
Building a rugged, solar-powered IoT Farm Hub to monitor soil/environment and send data to the cloud.

2. Hardware Architecture (VALIDATED)
Core: ESP32 (DOIT DevKit V1) + EC200U (4G LTE Modem).

Power Source: 3x 18650 Li-Ion Pack (3S) with BMS.

Status: TESTED & WORKING.

Regulation: LM2596 Buck Converter (Tuned to 5.1V).

Sensors: Capacitive Soil Moisture (Analog).

Board: 9x15cm Perfboard.

3. Power Sub-System & Charging Logic (CRITICAL UPDATES)
Solar Panel: Epoxy 12v 4W charging at 85mAh between 11AM to 3PM

Charge Controller (CC): Generic "Blue" PWM Controller.

Settings: Configured to Mode b03 (Li-Ion) with Cut-off Voltage set to 12.3V.

Safety Note: No external diodes needed. The 12.3V setting + natural voltage drop/reading error (CC reads ~0.2V higher than actual) ensures battery never exceeds safe 12.6V limit. UPDATE: Diode needed to avoid CC taking power back from battery when dark.

BMS "Wake-Up" Protocol:

Issue: BMS trips (shows 3V ghost voltage) if battery is connected to CC first due to capacitor inrush.

Solution: ALWAYS connect Solar Panel to CC first, then connect the Battery. This "jump starts" the BMS.

Wiring Plan:

Topology: "Common Bus" Strategy confirmed.

Grounds: Common.

4. Software & Cloud (VALIDATED)
Infrastructure: GCP Pipeline (Cloud Run -> BigQuery) is LIVE.

Status: Data upload validated successfully.

Firmware Logic:

Hub (Main): Wakes every 15 minutes.

Spoke 1: Wakes every 30 minutes.

Connectivity: EC200U 4G Modem confirmed working.

5. Physical Build Status
Enclosure: 200x150mm ABS IP65 box (Selected to replace cramped CCTV box).

Soldering: PENDING / NEXT STEP.

Awaiting/Received Bakon BK606A (90W) iron.

Tip Selection: 900M-TK (Knife) for battery pads, TB (Pencil) for ESP32.

6. Immediate Next Steps
Soldering (High Priority): Transfer the breadboard prototype to the Perfboard.

Order: Solder "Power Section" (Screw Terminals + LM2596 Headers) first to verify voltages before adding ESP32.

Enclosure Prep: Drill holes in the new 200x150mm box for glands/connectors.

Final Assembly: Mount PCB and Battery Pack (with "Seatbelt" tape and "Modem Kiss" insulation).

7. Long Term TODO:
Add a SMS featire so that HUB/Modem sends SMS as soon as it wakes up in the morning (this will act as a alert if no SMS is received)

########################
Current Logic Status
Project Freezer: IoT Farm Hub (ESP32 + EC200U + GCP)
Status: STABLE / PRODUCTION READY Date: January 3, 2026 Components: ESP32 (Hub), EC200U (LTE Modem), Cloud Run (Python), BigQuery, GCS.

1. CRITICAL LOGIC & "GOTCHAS" (Read this before changing anything)
A. The "Sticky Header" Bug (Quectel EC200U)
Issue: The modem's "Raw Header" mode (AT+QHTTPCFG="requestheader",1), required for Multipart Image uploads, is sticky. It persists across commands.

Symptom: After an image upload, simple GET requests (Sensor Data) fail immediately with ERROR because the modem is still expecting raw headers.

The Fix: You must explicitly DISABLE raw headers (AT+QHTTPCFG="requestheader",0) at the start of any function that uses standard GET/POST commands.

B. The "Dirty State" Reset
Issue: If an upload fails or times out, the modem often keeps the HTTP context open, blocking future attempts.

The Fix: A helper function resetHTTP() is called before every transaction.

Command: AT+QHTTPSTOP

Wait: 2 seconds.

C. Cloud Function "Traffic Control"
Logic: A single Cloud Run endpoint handles two distinct data types using request.method.

POST: Image Uploads -> Streams to Google Cloud Storage (GCS).

GET: Sensor Data -> Inserts into BigQuery.

Critical Python Fix: The imports use from datetime import datetime. Therefore, you must use datetime.utcnow() and NOT datetime.datetime.utcnow(). The latter causes a silent 500 crash.

D. The "Soda Straw" Delay
Logic: When piping the image from SPIFFS to the Modem, you cannot dump the file at full speed. The EC200U UART buffer will overflow.

The Fix: A delay(20) is added inside the while(file.available()) loop.
