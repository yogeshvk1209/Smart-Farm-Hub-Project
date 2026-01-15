# Field Testing Log: Solar ESP32 Hub

**Date:** January 2026  
**Location:** Roof (Bengaluru)  
**Status:** 48-72Hr Bake-in Phase  

## 1. System Specifications
* **Solar:** 4W Panel
* **Controller:** Generic CC -> BMS
* **Storage:** 3S Li-ion (3 x 18650, ~2000mAh per cell)
    * *Max Voltage:* 12.6V
    * *Nominal:* 11.1V
    * *Cutoff:* ~9.6V - 10.0V
* **Load:** ESP32 Dev Kit + CAPFU EC200U Modem (4G)
* **Software Config:**
    * Active Windows: 4 per hour (xx:58, xx:13, xx:28, xx:43)
    * Window Duration: 4 minutes
    * Sleep Mode: ESP Deep Sleep + Modem Power Down

---

## 2. Observation Log (Bake-in)

| Timestamp | Event / Condition | Voltage Reading | Delta | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Monday Afternoon** | Initial Deployment | **12.20 V** | - | Start |
| **Monday Evening** | End of Day 1 | **12.40 V** | +0.20 V | Charged |
| **Tuesday 7:00 AM** | Morning Wake | **12.20 V** | -0.20 V | Settling |
| **Tuesday 5:00 PM** | End of Day 2 (**Overcast**) | **11.50 V** | **-0.70 V** | **High Drain** |
| **Wednesday 9:00 AM**| Morning Wake | **11.46 V** | **-0.04 V** | **Stable** |

---

## 3. Performance Analysis

### A. Night Mode (Sleep Efficiency)
* **Data:** Tuesday Night drop was only **0.04V** (11.50V -> 11.46V).
* **Conclusion:** **Excellent.**
    * The hardware has no significant "vampire drain."
    * The `manageSleep()` function is correctly putting the ESP32 into deep sleep.
    * The modem is not leaking power overnight.

### B. Day Mode (Active Duty Cycle)
* **Data:** Tuesday (Overcast) saw a **0.7V** drop.
* **Conclusion:** **Aggressive Power Consumption.**
    * *Current Setting:* 16 minutes active per hour (26% Duty Cycle).
    * *Solar Input:* On a cloudy day, a 4W panel cannot sustain this duty cycle.
    * *Result:* The battery discharged significantly because Load > Solar Input.

### C. Battery Chemistry Context (3S Li-ion)
Current reading of **11.46V** places the battery in the **"Plateau Phase"** (~50-60% Capacity).

* **12.6V -> 12.2V (Surface Charge):** Voltage drops rapidly here. The 0.2V drop Monday night was mostly "fluffy" surface charge dissipating, not massive energy loss.
* **11.5V -> 11.46V (The Plateau):** Voltage is "stiffer" here. A 0.04V drop indicates very little energy was used.
* **Warning Zone:** If voltage hits **11.0V**, the curve steepens again. The drop from 11.0V to 10.0V will happen quickly.

---

## 4. Recommendations & Action Items

### Immediate Actions
1.  **Monitor:** Keep checking voltage. If it touches **11.1V**, consider manually charging or turning off until sunny.
2.  **Validation:** Continue logging to confirm the "Plateau" behavior holds for another 24 hours.

### Software Improvements (Next OTA/Flash)
1.  **Dynamic Windows:**
    * *Current:* Fixed 4 mins duration.
    * *Proposed:* If `V < 11.8V`, reduce window to **1 minute** or skip every second upload.
2.  **Modem Hygiene:**
    * Ensure `AT+CPOWD=1` or equivalent is sent before Deep Sleep to guarantee the modem isn't just in "Standby."
