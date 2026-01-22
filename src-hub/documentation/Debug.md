# FarmHub Modem Debugging Guide
**Target Hardware:** Quectel EC200U (LTE Cat 1)
**Network:** Jio 4G (India) - Pure IPv6 / LTE-Only
**Controller:** ESP32

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
*Being connected to the tower (Layer 1) is different from having Internet (Layer 3).*

### Step A: Configure TCP Stack
*We must tell the internal TCP/IP stack to use the IPv6 context.*

1.  **Deactivate first (Unlock settings):**
    ```text
    AT+QIDEACT=1
    ```
2.  **Set Context 1 to IPv4v6:**
    ```text
    AT+QICSGP=1,3,"jionet","","",0
    ```
    *(Note: The `3` stands for IPV4V6. Use `1` for IPv4, `2` for IPv6).*

### Step B: Activate & Verify
1.  **Activate Context:**
    ```text
    AT+QIACT=1
    ```
2.  **Check IP Address:**
    ```text
    AT+CGPADDR=1
    ```
    * **Success:** `+CGPADDR: 1,"0.0.0.0,2409:..."` (IPv6 address present).
    * **Fail:** `0.0.0.0,0:0:0...` (No IP allocated).

---

## 4. Connectivity Test (Ping)
*The ultimate proof that data is flowing.*

| Command | Description |
| :--- | :--- |
| `AT+QPING=1,"www.google.com"` | Pings Google. Look for time in ms (e.g., `32`). |
| `AT+QPING=1,"8.8.8.8"` | **Note:** This often **FAILS on Jio** because `8.8.8.8` is an IPv4 address, and we are on a pure IPv6 stack. Use a hostname (google.com) instead. |

---

## 5. HTTP/SSL Debugging (Advanced)
*If the ESP32 code says "Connect Failed" or "CME ERROR".*

| Command | Description |
| :--- | :--- |
| `AT+QHTTPURL=?` | Check max URL length supported. |
| `AT+QSSLCFG="seclevel",1,0` | Set SSL verification level (0 = No Verify/Insecure). Useful if certificates fail. |
| `AT+QIGETERROR` | Returns the specific error code for the last failed TCP/IP command. |

---

## 6. Full Factory Reset
*Use only if the modem is behaving erratically.*
```text
AT&F
