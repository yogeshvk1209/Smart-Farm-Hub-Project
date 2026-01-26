Submersible Spoke (Motor 1)
Overview
This Spoke provides remote-controlled timing for a 3-phase AC submersible pump. It uses the presence of AC Mains (via the Hi-Link power supply) as a primary "Power Restored" signal and an SCT-013 current sensor for secondary "Motor Running" verification.The Logic: "Mains-Aware Hunter"Boot = Power: Since the node is powered by the Hi-Link on the AC line, every boot signifies that the AC Mains are ON.First Handshake: Upon booting, the Spoke pings the Hub to announce it is online.Command Execution: The Spoke stays in a low-power "Hunter" loop until you send an SMS command via the Hub (e.g., "Motor1 ON 5").Timer Safety: The motor run-time is managed locally via millis(). If current is not detected within 10 seconds of the relay closing, the Spoke aborts the job and alerts the Hub.Operational StatesMains Detection: Hub receives a ping from ID 3 $\rightarrow$ Hub sends "⚡ Farm Power Restored" SMS.Running Success: Relay closed + Current detected $\rightarrow$ Hub sends "✅ SUCCESS: Submersible is running" SMS.Mains/Starter Failure: Relay closed + No current $\rightarrow$ Hub sends "⚠️ ALERT: No current detected" SMS.

2. Bill of Materials (BOM)
ComponentQtySpecifications & Notes
Microcontroller1Seeed Studio XIAO ESP32-C3.AC-DC Module1Hi-Link HLK-PM01 (5V/3W) or HLK-PM03 (3.3V).Current Sensor1SCT-013-030 (30A/1V). Split-core for non-invasive phase monitoring.Relay Module15V 10A Single-Channel Relay (Optoisolated).Resistors210kΩ and 10kΩ (for a voltage divider if using a 5V sensor output on a 3.3V pin).Capacitor110µF Electrolytic (Optional: to stabilize CT sensor analog readings).Enclosure1IP65 Junction Box (to be mounted near the pump starter panel).

3. Detailed Wiring Diagram
Code snippet

graph TD
    subgraph "High Voltage (AC Mains)"
        AC_L[AC Live] --> HL_IN[Hi-Link AC-IN]
        AC_N[AC Neutral] --> HL_IN
        AC_L --> Relay_COM[Relay COM]
        Relay_NO[Relay NO] --> Pump_Starter[Pump Starter Panel]
    end

    subgraph "Low Voltage (Controller)"
        HL_OUT[Hi-Link 5V/3.3V OUT] --> XIAO_VIN[XIAO 5V/3V3 Pin]
        HL_GND[Hi-Link GND] --> XIAO_GND[XIAO GND]
        
        XIAO_GND --> Relay_GND[Relay GND]
        XIAO_D0[XIAO GPIO 2 / D0] --> Relay_IN[Relay Signal IN]
        XIAO_VIN --> Relay_VCC[Relay VCC]
        
        SCT[SCT-013 CT Sensor] -->|Clamp around One Phase| AC_L
        SCT -->|Signal| XIAO_A0[XIAO GPIO 0 / A0]
    end

    subgraph "Safety & Sensing"
        XIAO_A0 --- R1[Resistor 10k]
        R1 --- XIAO_GND
    end

    style AC_L fill:#ff9999,stroke:#333
    style HL_OUT fill:#99ff99,stroke:#333
    style XIAO_VIN fill:#99ccff,stroke:#333
Wiring Instructions:

Power: Wire the Hi-Link directly to the 230V AC input. This ensures the XIAO boots as soon as the farm gets power.

Relay: Wire the Normally Open (NO) and Common (COM) terminals across your pump's manual "Start" button or contactor coil.

Current Sensor: Clamp the SCT-013 around only one of the three wires going to the pump. Connect the 3.5mm jack output to the XIAO's A0 pin via the voltage divider.
