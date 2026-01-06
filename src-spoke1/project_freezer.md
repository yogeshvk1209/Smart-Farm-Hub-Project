# Project Freezer: Spoke 1 (The Sensor Dispenser)
**Date:** January 06, 2026
**Status:** ⏸️ PAUSED (Waiting for BOM Shipment)
**Next Action:** Solder Power Section (TP4056 + Battery + LDO).

---

## 1. Project Overview
**Goal:** A self-sustaining, solar-powered soil moisture monitoring node.
**Architecture:**
* **Power:** 6V Solar $\rightarrow$ TP4056 Charger $\rightarrow$ 1x 18650 Battery $\rightarrow$ MCP1700 LDO $\rightarrow$ ESP8266.
* **Logic:** ESP8266 wakes up via DS3231 RTC, reads sensor, transmits data, and returns to Deep Sleep.
* **Lifespan:** Designed for months/years of autonomy without maintenance.

---

## 2. The "Golden" Bill of Materials (Locked)
* **MCU:** ESP8266 NodeMCU v3 (Mounted on Female Headers).
* **Time:** DS3231 RTC Module (Modified).
* **Sensor:** Capacitive Soil Moisture Sensor v1.2 (Black).
* **Power Mgmt:**
    * TP4056 USB-C/Micro (Must have `OUT+`/`OUT-` protection pads).
    * **MCP1700-3302E** Voltage Regulator (TO-92 Package).
    * **1000µF Electrolytic Capacitor** (10V/16V) - *The WiFi Surge Tank*.
    * **100nF (104) Ceramic Capacitor** - *Noise Filter*.
* **Energy:** 1x 18650 Li-Ion (2000mAh) + 6V 100mA Solar Panel.
* **Housing:** IP65 Box + PVC Pole + Araldite Epoxy (for sensor).

---

## 3. Hardware Implementation Guide

### A. The Circuit Diagram (Mental Model)
**Flow:** `[Solar]` $\rightarrow$ `[TP4056]` $\rightarrow$ `[Battery]` $\rightarrow$ `[MCP1700 + Caps]` $\rightarrow$ `[ESP8266]`

### B. Pinout & Wiring Map
| Component Pin | Connect To | Notes |
| :--- | :--- | :--- |
| **MCP1700 Left** | GND | Flat face towards you. |
| **MCP1700 Mid** | TP4056 OUT+ | Input from Battery System. |
| **MCP1700 Right** | ESP 3.3V | Output to MCU. |
| | | |
| **RTC VCC** | ESP 3.3V | Constant Power. |
| **RTC GND** | GND | Common Ground. |
| **RTC SDA** | D2 (GPIO 4) | I2C Data. |
| **RTC SCL** | D1 (GPIO 5) | I2C Clock. |
| | | |
| **Sensor VCC** | **D5 (GPIO 14)** | **Switched Power** (Only ON during reading). |
| **Sensor GND** | GND | Common Ground. |
| **Sensor Data** | A0 (ADC0) | Analog Signal. |

### C. The 7x5 Perf Board Layout
* **Left Side:** Power Plant (TP4056, Screw Terminals, Capacitors, LDO).
* **Right Side:** Brains (NodeMCU on female headers).
* **Bottom/Middle:** Time (DS3231).
* **Wiring Flow:** U-Shape (Solar enters Left $\rightarrow$ Regulates Bottom-Left $\rightarrow$ Powers Right).

---

## 4. Critical Modifications (Do Not Skip)

### ⚠️ 1. The DS3231 RTC Vampire Fix
The standard ZS-042 module tries to charge the battery. This is dangerous for standard CR2032 cells and wastes power.
* **Action:** Locate the resistor labeled **201** or the Diode near the battery holder. **Desolder or cut it.**
* **Action:** Desolder the "Power" LED on the module to save ~3mA.

### ⚠️ 2. The Sensor Waterproofing
The "waterproof" sensor is only waterproof at the bottom.
* **Action:** Remove the white connector. **Direct solder** wires to pads.
* **Action:** Encase the entire top section (chips + solder joints) in **Araldite Epoxy**.
* **Action:** Slide Heat Shrink over the wet epoxy for a perfect seal.

### ⚠️ 3. The WiFi Surge Protection
The MCP1700 LDO is efficient but weak (250mA max).
* **Action:** You **MUST** solder the **1000µF Capacitor** across the 3.3V and GND rails, as close to the ESP8266 as possible. Without this, the unit will crash (brownout) every time WiFi starts.

---

## 5. Software Logic Summary
* **Wake Up:** Triggered by RTC Alarm (Pin D0 connected to RST for Deep Sleep, or just RTC Interrupt). *Note: Currently using Deep Sleep timer logic.*
* **Step 1:** Turn **D5 HIGH** (Power up sensor).
* **Step 2:** Wait **100ms** (Warm up).
* **Step 3:** Read **A0** (Take multiple samples and average).
* **Step 4:** Turn **D5 LOW** (Cut power to sensor immediately).
* **Step 5:** Connect WiFi $\rightarrow$ Send to Hub/BigQuery.
* **Step 6:** Calculate next wake time $\rightarrow$ Deep Sleep.

---

## 6. Revival Checklist (When Parts Arrive)
1.  [ ] **Solder Power First:** Mount TP4056, MCP1700, and Capacitors.
2.  [ ] **Test Power:** Connect Battery. Measure output at LDO pin. Should be **3.3V stable**.
3.  [ ] **Mount Logic:** Solder Female Headers. Insert NodeMCU.
4.  [ ] **Code Test:** Upload "Blink" code but change pin to D5 (Sensor Power Pin) to verify you can toggle voltage.
5.  [ ] **Full Integration:** Connect Sensor and RTC. Run the "Heartbeat" code.
