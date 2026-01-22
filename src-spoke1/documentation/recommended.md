# Recommended Hardware Improvements: Spoke 1

**Status:** 🟡 RECOMMENDED IMPROVEMENT
**Component:** Voltage Regulation (Battery -> ESP8266)

## 1. The Issue: "The Silicon Hack"
*   **Current Setup:** `Li-Ion Battery (4.2V - 3.7V)` $\rightarrow$ `Diode` $\rightarrow$ `ESP8266 (3.3V)`.
*   **Theory:** A diode drops ~0.7V. $4.2V - 0.7V = 3.5V$ (Safe).
*   **The Risk:**
    1.  **Low Load Spike:** At deep sleep currents (~20µA), a standard diode's voltage drop can decrease to **~0.5V**. This puts **3.7V** on the ESP8266 pin. The absolute maximum rating is **3.6V**. This risks long-term damage.
    2.  **Brownout:** When the battery is low (3.6V), the voltage at the ESP is ~2.9V, which may cause instability or WiFi failure.

## 2. The Fix: Low-Dropout Regulator (LDO)
Replace the diode with a dedicated LDO that has extremely low quiescent current.

### 🔧 Recommended Parts
*   **MCP1700-3302E:** 3.3V Output, 250mA, **1.6µA** Quiescent Current.
*   **HT7333-A:** 3.3V Output, **4µA** Quiescent Current.

### Advantages
*   **Stable 3.3V:** Delivers exactly 3.3V whether the battery is fresh (4.2V) or dying (3.5V).
*   **Safety:** Zero risk of over-voltage frying the chip.
*   **Efficiency:** Consumes negligible power compared to the ESP8266's internal regulator or the leakage of a generic diode at wrong operating points.

### 🔌 Wiring Change
*   **Remove:** The Diode between TP4056 and ESP.
*   **Add:** LDO Regulator.
    *   **GND** $\rightarrow$ Common Ground.
    *   **VIN** $\rightarrow$ TP4056 OUT+.
    *   **VOUT** $\rightarrow$ ESP8266 3.3V Pin.
    *   *Note:* Add a 100µF capacitor on VOUT for WiFi peak stability.
