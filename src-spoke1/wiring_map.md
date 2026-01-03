# 🗺️ The Final Wiring Map (Solder Guide)

This guide details the connections for the Solar-Powered Spoke node, including power harvesting, battery storage, and controller connections.

## 1. Solar Side (Harvest)
*   **Solar Panel (+)** $\rightarrow$ **Diode A** (Anode/Black Side)
*   **Diode A** (Cathode/Silver Band) $\rightarrow$ **TP4056 IN+**
*   **Solar Panel (-)** $\rightarrow$ **TP4056 IN-**

## 2. Battery Side (Storage)
*   **Battery (+)** $\rightarrow$ **TP4056 B+**
*   **Battery (-)** $\rightarrow$ **TP4056 B-**

## 3. NodeMCU Side (Consumption)
*   **TP4056 OUT+** $\rightarrow$ **Diode B** (Anode/Black Side)
*   **Diode B** (Cathode/Silver Band) $\rightarrow$ **NodeMCU 3.3V Pin** *(Crucial Step: Do not connect to 5V/Vin)*
*   **TP4056 OUT-** $\rightarrow$ **NodeMCU GND**

## 4. The "Brain" Connections (Sensors & Sleep)
*   **Deep Sleep Wake:** D0 (GPIO16) $\rightarrow$ RST
*   **RTC (DS3231):**
    *   VCC $\rightarrow$ 3.3V
    *   GND $\rightarrow$ GND
    *   SDA $\rightarrow$ D2 (GPIO4)
    *   SCL $\rightarrow$ D1 (GPIO5)
*   **Soil Sensor:**
    *   VCC $\rightarrow$ 3.3V
    *   GND $\rightarrow$ GND
    *   AOUT $\rightarrow$ A0
