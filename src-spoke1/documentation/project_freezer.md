# Project Freezer: Spoke 1 (The Sensor Dispenser)
**Date:** January 22, 2026
**Status:** ⏸️ PAUSED (Waiting for Final Assembly)
**Next Action:** Verify Power Chain with new Buck Converter.

---

## 1. Project Overview
**Goal:** A self-sustaining, solar-powered soil moisture monitoring node.
**Architecture:**
* **Power:** `4W Solar` $\rightarrow$ `Buck (5.1V)` $\rightarrow$ `TP4056` $\rightarrow$ `1x 18650`.
* **Regulation:** `Battery` $\rightarrow$ `Diode` (See *recommended.md*) $\rightarrow$ `ESP8266`.
* **Logic:** ESP8266 wakes up via DS3231 RTC, reads sensor, transmits data, and returns to Deep Sleep.

---

## 2. The "Golden" Bill of Materials (Locked)
* **MCU:** ESP8266 NodeMCU v3.
* **Time:** DS3231 RTC Module.
* **Sensor:** Capacitive Soil Moisture Sensor v1.2.
* **Power Mgmt:**
    * **LM2596 Buck Converter** (Pre-regulator).
    * **TP4056** USB-C/Micro (Charger).
    * **Diode IN5408S** (Voltage Drop) - *Ideally replace with LDO*.
* **Energy:** 1x 18650 Li-Ion (2500mAh) + 4W Solar Panel.

---

## 3. Hardware Implementation Guide

### A. The Circuit Diagram (Mental Model)
**Flow:** `[Solar]` $\rightarrow$ `[Buck 5.1V]` $\rightarrow$ `[TP4056]` $\rightarrow$ `[Battery]` $\rightarrow$ `[Diode]` $\rightarrow$ `[ESP8266]`

### B. Pinout & Wiring Map
| Component Pin | Connect To | Notes |
| :--- | :--- | :--- |
| **Solar +** | LM2596 IN+ | |
| **LM2596 OUT+** | TP4056 IN+ | Tuned to 5.1V. |
| **TP4056 B+** | Battery + | |
| **TP4056 OUT+** | Diode Anode | |
| **Diode Cathode** | ESP 3.3V | *See recommended.md* |
| | | |
| **RTC VCC** | ESP 3.3V | |
| **RTC SDA** | D2 (GPIO 4) | |
| **RTC SCL** | D1 (GPIO 5) | |

### C. Layout Strategy
* **Input:** Solar enters LM2596 first.
* **Charging:** Clean 5.1V feeds TP4056.
* **Logic:** Battery output powers ESP via Diode drop.

---

## 4. Critical Modifications (Do Not Skip)

### ⚠️ 1. The DS3231 RTC Vampire Fix
The standard ZS-042 module tries to charge the battery.
* **Action:** Desolder/cut the diode or resistor (201) near the battery holder.

### ⚠️ 2. The Sensor Waterproofing
* **Action:** Encase the entire top section (chips + solder joints) in **Araldite Epoxy**.

### ⚠️ 3. The WiFi Surge Protection
* **Action:** You **MUST** solder a **1000µF Capacitor** across the 3.3V and GND rails, as close to the ESP8266 as possible.

---

## 5. Software Logic Summary
* **Wake Up:** Triggered by RTC Alarm/Timer.
* **Step 1:** Turn **D5 HIGH** (Power up sensor).
* **Step 2:** Wait **800ms**.
* **Step 3:** Read **A0**.
* **Step 4:** Turn **D5 LOW**.
* **Step 5:** Connect WiFi $\rightarrow$ Send to Hub.
* **Step 6:** Sleep.

---

## 6. Revival Checklist
1.  [ ] **Tune Buck:** Set LM2596 to 5.1V *before* connecting TP4056.
2.  [ ] **Solder Power:** Connect Solar -> Buck -> TP4056 -> Battery.
3.  [ ] **Check Voltage:** Verify voltage at ESP pins does not exceed 3.6V.
