# 🗺️ The Final Wiring Map (Solder Guide)

This guide details the connections for the Solar-Powered Spoke node, including power harvesting, battery storage, and controller connections.

## 1. Solar Side (Harvest)
*   **Solar Panel (+)** $\rightarrow$ **LM2596 IN+**
*   **Solar Panel (-)** $\rightarrow$ **LM2596 IN-**
*   **LM2596 OUT+** (Tuned to 5.1V) $\rightarrow$ **TP4056 IN+**
*   **LM2596 OUT-** $\rightarrow$ **TP4056 IN-**

## 2. Battery Side (Storage)
*   **Battery (+)** $\rightarrow$ **TP4056 B+**
*   **Battery (-)** $\rightarrow$ **TP4056 B-**

## 3. NodeMCU Side (Consumption)
*   **TP4056 OUT+** $\rightarrow$ **Diode IN5408S** (Anode)
*   **Diode IN5408S** (Cathode) $\rightarrow$ **NodeMCU 3.3V Pin**
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
