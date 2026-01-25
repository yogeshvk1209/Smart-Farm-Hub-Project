# FarmHub Modem Debugging Guide
**Target Hardware:** Quectel EC200U (LTE Cat 1)
**Network:** Jio 4G (India) - Pure IPv6 / LTE-Only
**Controller:** ESP32
**Firmware Version:** V5.0.0

## 0. Prerequisite: The Debug Tool
To execute the AT commands listed below, use the provided passthrough tool:
1.  Flash `src/gsm_testing_main.cpp` (Rename it to `src/main.cpp` temporarily).
2.  Open Serial Monitor at **115200 baud**.
3.  Type the commands below directly into the console.

---

## 1. Quick Health Check (Sanity Test)
*If these fail, check wiring (RX/TX crossed?) and Power (Battery connected?).*

| Command | Expected Output | Meaning |
| :--- | :--- | :--- |
| `AT` | `OK` | Modem is alive and listening. |
| `ATI` | `Quectel EC200U...` | Confirm modem model/firmware. |
| `AT+CSQ` | `+CSQ: 15,99` (or >10) | Signal Strength (0-31). Higher is better. |
| `AT+CPIN?` | `+CPIN: READY` | SIM card is inserted and unlocked. |

---

## 2. Network Registration (The "Jio Special")
*Jio is unique because it often rejects pure IPv4 requests and requires roaming enabled on some IoT firmware.*

### Step A: Status Check
| Command | Expected Output | Meaning |
| :--- | :--- | :--- |
| `AT+CEREG?` | `0,1` (Home) or `0,5` (Roaming) | **Success.** Connected to Tower. |
| | `0,2` | Searching... (Not connected yet). |
| | `0,0` | Idle/Not searching. |
| `AT+COPS?` | `+COPS: 0,0,"IND-JIO",7` | Connected to Jio 4G (7=LTE). |

### Step B: The Fix (If stuck on `0,2` Searching)
*Run these in order if the modem sees the signal but won't register.*

1.  **Force Roaming (Crucial for some circles):**
    ```text
    AT+QCFG="roamservice",2,1
    ```
2.  **Force IPv4+IPv6 Context (Jio rejects pure IPv4):**
    ```text
    AT+CGDCONT=1,"IPV4V6","jionet"
    ```
3.  **Reboot Modem:**
    ```text
    AT+CFUN=1,1
    ```

---

## 3. Data Activation (Getting an IP)
*We must tell the internal TCP/IP stack to use the IPv6 context.*

1.  **Deactivate first (Unlock settings):**
    ```text
    AT+QIDEACT=1
    ```
2.  **Set Context 1 to IPv4v6:**
    ```text
    AT+QICSGP=1,3,"jionet"
    ```
    *(Note: V5.0.0 uses simplified params. The `3` stands for IPV4V6).*

3.  **Activate Context:**
    ```text
    AT+QIACT=1
    ```
4.  **Check IP Address:**
    ```text
    AT+CGPADDR=1
    ```
    * **Success:** `+CGPADDR: 1,"0.0.0.0,2409:..."` (IPv6 address present).

---

## 4. Image Upload Debugging (New in V5.0.0)
*The system now changes baud rates during upload. Use these commands to simulate that flow.*

1.  **Set Baud to 57600 (Image Mode):**
    ```text
    AT+IPR=57600
    ```
    *(Note: You must change your Serial Monitor baud rate to 57600 immediately after this).*

2.  **Restore Baud to 115200 (Telemetry Mode):**
    ```text
    AT+IPR=115200
    ```
    *(Note: Switch Serial Monitor back to 115200).*

3.  **Disable Echo (Used during upload):**
    ```text
    ATE0
    ```
    *(Note: You won't see what you type anymore).*

---

## 5. HTTP/SSL Debugging (Advanced)
*If the ESP32 code says "Connect Failed" or "CME ERROR".*

| Command | Description |
| :--- | :--- |
| `AT+QHTTPURL=?` | Check max URL length supported. |
| `AT+QHTTPPOST=?` | Check POST support. |
| `AT+QIGETERROR` | Returns the specific error code for the last failed TCP/IP command. |

---

## 6. Full Factory Reset
*Use only if the modem is behaving erratically.*
```text
AT&F