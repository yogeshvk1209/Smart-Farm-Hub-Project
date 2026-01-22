# BOM: Spoke 1 - Solar Soil Sensor Unit
**Version:** Final Locked (Jan 2026)
**Power Architecture:** 1S Li-Ion (3.7V) -> LDO (3.3V)

## 1. Electronics (Core Logic)
| Component | Qty | Specifications & Notes |
| :--- | :--- | :--- |
| **Microcontroller** | 1 | **ESP8266 NodeMCU v3** (or Wemos D1 Mini). |
| **RTC Module** | 1 | **DS3231 (ZS-042)**. <br>⚠️ **MOD REQUIRED:** Remove Power LED and Charging Resistor (labeled "201") to prevent battery drain. |
| **Soil Sensor** | 1 | **Capacitive Soil Moisture Sensor v1.2**. <br>⚠️ **NOTE:** Must be the black corrosion-resistant type. Do NOT use the shiny fork resistive type. |
| **Perf Board** | 1 | **7cm x 5cm** (Double-sided FR4 recommended). |
| **Headers** | 1 Pair | **Female Header Strip (1x15)**. <br>*Used to mount NodeMCU so it remains removable.* |

## 2. Power System (Charging & Regulation)
| Component | Qty | Specifications & Notes |
| :--- | :--- | :--- |
| **Solar Panel** | 1 | **6V 100mA** (Small form factor). |
| **Battery** | 1 | **18650 Li-Ion Cell** (Min 2000mAh). |
| **Battery Holder** | 1 | 1-Slot 18650 Holder (or spot-welded tabs). |
| **Charger Module** | 1 | **TP4056 (USB-C or Micro-USB)**. <br>⚠️ **CRITICAL:** Must have `OUT+` and `OUT-` pads (Built-in Battery Protection). |
| **Voltage Regulator**| 1 | **MCP1700-3302E**. <br>• **Package:** TO-92 (3-pin transistor style). <br>• **Spec:** Fixed 3.3V, Low Quiescent Current (1.6µA). |
| **Capacitor (Bulk)** | 1 | **1000µF Electrolytic** (Rated 10V, 16V, or 25V). <br>*Essential for WiFi stability.* |
| **Capacitor (Filter)**| 1 | **100nF (0.1µF) Ceramic** (Code: 104). <br>*Noise filtration for the regulator.* |

## 3. Wiring & Connection
| Component | Qty | Specifications & Notes |
| :--- | :--- | :--- |
| **Sensor Wire** | 1.5m | **3-Core Stranded Wire** (22AWG - 24AWG). <br>*Tip: Old USB data cables work perfectly (Red/Black/Green).* |
| **Hookup Wire** | - | Solid core or stranded (22AWG) for internal board wiring. |
| **Terminals** | 3 | **2-Pin PCB Screw Terminals** (5.08mm or 5mm pitch). <br>*For: Solar Input, Battery Connection, Sensor Output.* |

## 4. Mechanical & Enclosure
| Component | Qty | Specifications & Notes |
| :--- | :--- | :--- |
| **Enclosure** | 1 | **IP65 Waterproof Junction Box** (approx 4" x 4"). <br>*Must fit the 18650 holder inside.* |
| **Mounting Pole** | 1m | **PVC Pipe** (20mm or 25mm diameter). |
| **Clamps** | 2 | **U-Clamps / Saddle Clamps** (to match PVC pipe size). |
| **Cable Glands** | 2 | **PG7 or PG9**. <br>*Waterproof entry for Solar wires and Sensor cable.* |

## 5. Consumables (Waterproofing)
| Component | Qty | Specifications & Notes |
| :--- | :--- | :--- |
| **Epoxy** | 1 | **Araldite** (Standard or 5-Min). <br>⚠️ **CRITICAL:** Use to pot the *top electronics* of the soil sensor after soldering. |
| **Coating** | 1 | **Clear Nail Polish** or Acrylic Spray. <br>*To coat the PCB traces and sensor edges.* |
| **Sealant** | 1 | **Hot Glue** or RTV Silicone. |

---

## ⚡ Important Assembly Checks
1.  **LDO Pinout (MCP1700 TO-92):** Face flat side to you. Left=GND, Middle=VIN, Right=VOUT.
2.  **Capacitor Polarity:** 1000µF "Stripe" side goes to GND.
3.  **Sensor Logic:** Do NOT power sensor from permanent 3.3V. Wire Sensor VCC to a GPIO pin (e.g., D5) to turn it on only when reading.
4.  **Waterproofing:** The Capacitive Sensor v1.2 is **NOT** waterproof at the top. You must Epoxy the solder joints.
