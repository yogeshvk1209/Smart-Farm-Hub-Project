# FarmSpoke: Soil Moisture Node (ESP8266)

The **FarmSpoke** is a low-power, wireless sensor node designed to sit in the field. It reads environmental data and broadcasts it to the Hub via ESP-NOW.

## 🧠 Hardware Architecture
* **Controller:** ESP8266 (NodeMCU / Wemos D1 Mini)
* **Sensor:** Capacitive Soil Moisture Sensor v1.2 (Corrosion Resistant)
* **Protocol:** ESP-NOW (Peer-to-Peer, Connectionless)

## 🔌 Pinout & Wiring

```text
     ESP8266 (NodeMCU)             Soil Sensor
    +---------+                   +-----------+
    |     3V3 |-------------------| VCC       |
    |     GND |-------------------| GND       |
    |      A0 |<------------------| AOUT      |
    +---------+                   +-----------+
```

| ESP8266 Pin | Sensor Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **A0** (ADC) | AOUT | Analog Output | Reads voltage (Dry=High, Wet=Low) |
| **3V3** | VCC | Power | Constant 3.3V |
| **GND** | GND | Ground | |

*(Future v2 plan: Move VCC to a GPIO pin to power down sensor between readings for battery savings)*

## ⚙️ Calibration
The code maps raw analog values (0-1024) to a percentage (0-100%).

* **Air Value (0%):** ~1024 (Max Resistance)
* **Water Value (100%):** ~420 (Min Resistance)
* **Formula:** `map(reading, 1024, 420, 0, 100)`

## ⚠️ Configuration Required
You **must** update the `broadcastAddress` in `src/main.cpp` to match your Hub's MAC address. The Hub prints its MAC address to the Serial Monitor upon startup.

## 📡 Communication Protocol
Uses **ESP-NOW** for ultra-fast transmission (<200ms active time).
* **Role:** Controller (Sender)
* **Target:** Broadcasts to Hub MAC Address.
* **Payload Structure:**
    ```cpp
    struct struct_message {
      int id;        // Unique Spoke ID (e.g., 1)
      int moisture;  // Raw Analog Value
    }
    ```

## 🔋 Power Management (Current Status)
*   **Current:** Uses standard `delay(60000)` loop for testing.
*   **Planned (v2):** Deep Sleep implementation with transistor-gated sensor power to prevent corrosion and extend battery life.
