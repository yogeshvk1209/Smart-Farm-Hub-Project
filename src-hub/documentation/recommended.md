# Recommended Hardware Improvements: The Hub

**Status:** ⚠️ CRITICAL SAFETY UPDATE REQUIRED
**Component:** Charging Circuit (3S Li-Ion BMS)

## 1. The Issue: Unsafe Charging Voltage
*   **Current Setup:** `Buck (13.9V)` $\rightarrow$ `Diode` $\rightarrow$ `BMS Input`.
*   **The Risk:** The 3S Battery Pack is fully charged at **12.6V** (4.2V per cell). Feeding it **~13.2V** (13.9V - 0.7V Diode Drop) relies entirely on the BMS to cut off charging.
    *   BMS modules are designed as a **Safety Cutoff** (Last Line of Defense), not a primary charge controller.
    *   If the BMS MOSFET fails short, the cells will be overcharged, leading to potential fire or venting.

## 2. The Fix: Precision Voltage Tuning
We must limit the input voltage to the safe maximum of the battery pack.

### 🔧 Action Plan
1.  **Disconnect the Battery** from the BMS/Circuit.
2.  **Measure Voltage:** Place a multimeter at the **BMS Input** pads (After the Diode).
3.  **Tune Buck Converter:** Adjust the first LM2596 potentiometer until the multimeter reads exactly **12.6V**.
4.  **Reconnect:** Once verified, reconnect the battery.

### Result
The Buck Converter now acts as a Constant Voltage (CV) source.
*   **Empty Battery:** Current flows freely (Constant Current phase limited by Buck/Panel).
*   **Full Battery (12.6V):** Voltage potential equalizes, and current flow naturally drops to near zero.
*   **Safety:** Even if the BMS fails, the voltage physically cannot exceed 12.6V.
