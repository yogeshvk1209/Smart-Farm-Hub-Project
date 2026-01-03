# FarmSpoke: Soil Moisture Node (ESP8266)

The **FarmSpoke** is a solar-powered, wireless sensor node designed for long-term field deployment. It reads soil moisture, manages its own power schedule via an RTC, and broadcasts data to the Hub via ESP-NOW.

## 🧠 Hardware Architecture
*   **Controller:** ESP8266 (NodeMCU / Wemos D1 Mini)
*   **Sensor:** Capacitive Soil Moisture Sensor v1.2 (Corrosion Resistant)
*   **Timekeeping:** DS3231 RTC (Real Time Clock) - I2C
*   **Power:**
    *   6V Solar Panel
    *   TP4056 Charge Controller
    *   18650 Li-Ion Battery
*   **Protocol:** ESP-NOW (Peer-to-Peer, Connectionless)

## 🔌 Pinout & Wiring

For detailed power and solar wiring instructions, see [wiring_map.md](wiring_map.md).

| ESP8266 Pin | Component Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **A0** (ADC) | Sensor AOUT | Analog Input | Reads moisture (Dry=High, Wet=Low) |
| **D1** (GPIO5) | RTC SCL | I2C Clock | |
| **D2** (GPIO4) | RTC SDA | I2C Data | |
| **D0** (GPIO16)| RST | Deep Sleep Wake | **REQUIRED** for waking up |
| **3V3** | VCC | Power | Powers Sensor & RTC |
| **GND** | GND | Ground | Common Ground |

## ⚙️ Configuration & Calibration

### 1. MAC Address
Update `broadcastAddress` in `src/main.cpp` to match your Hub's MAC address.
```cpp
uint8_t broadcastAddress[] = {0xC0, 0xCD, 0xD6, ...};
```

### 2. Soil Calibration
The code uses "zones" to interpret raw analog readings.
*   **Bone Dry (0%):** `DRY_SOIL` (Default: 700)
*   **Fully Wet (100%):** `WET_SOIL` (Default: 500)
*   **Logic:** Values > 700 are clipped to 0%. Values < 500 are clipped to 100%.

### 3. Sleep Schedule
The node uses the RTC to determine whether to sleep for a short interval or through the night.
*   **Start Hour:** 7 (07:00 AM)
*   **End Hour:** 19 (07:00 PM)
*   **Wake Logic (Day):** "Snap-to-Grid". The node calculates sleep seconds to wake up exactly at the next `:00` or `:30` minute mark (e.g., 10:00, 10:30).
*   **Night Mode:** Sleeps continuously from `END_HOUR` until `START_HOUR` the next day.

## 📡 Communication Protocol
*   **Role:** Controller (Sender)
*   **Payload Structure:**
    ```cpp
    struct struct_message {
      int id;        // Node ID (e.g., 1)
      int moisture;  // Percent Value (0-100)
      float voltage; // Battery Voltage (Currently sending 0.0 placeholder)
    }
    ```

## 🔋 Power Management
*   **Deep Sleep:** The ESP8266 enters Deep Sleep between readings to minimize consumption.
*   **RTC Wake:** The DS3231 allows the node to know "wall clock" time, enabling the Day/Night logic.
*   **Solar Charging:** The TP4056 manages battery charging from the solar panel.