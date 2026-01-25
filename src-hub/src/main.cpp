/**
 * FARM HUB V5.0.0 - DEADLOCK-FREE VERBOSE
 * Fixed: Guaranteed Mutex Unlock and Serial Debugging
 */

#define TINY_GSM_MODEM_SIM7600
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <RTClib.h>
#include <TinyGsmClient.h>
#include "secrets.h"

// --- HARDWARE CONFIG ---
#define MODEM_PWRKEY 18 
#define HUB_BAT_PIN 34
const float VOLT_FACTOR = 11.24; 
const int MAX_IMG_SIZE = 60000; 

// --- NIGHT MODE SCHEDULE (24H Format) ---
const int NIGHT_SLEEP_START = 19; // 7:00 PM
const int NIGHT_SLEEP_END   = 7;  // 7:00 AM

// --- GLOBALS ---
HardwareSerial modemSerial(2);
TinyGsm modem(modemSerial);
RTC_DS3231 rtc;

typedef struct struct_message {
    int id;
    int moisture;
    float voltage;
} struct_message;

volatile bool newSensorData = false;
volatile bool isModemBusy = false; 
struct_message incomingData;

uint8_t *imgBuffer;
volatile size_t imgSize = 0;
volatile unsigned long lastImgPacketTime = 0;

// --- PROTOTYPES ---
void IRAM_ATTR OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *data, int len);
void uploadTelemetry(struct_message* data);
void uploadImage();
float readHubBattery();
void sendStartupSMS();

// --- SETUP ---
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n=== HUB STARTING V5.0.2 [STABLE] ===");

    imgBuffer = (uint8_t *)malloc(MAX_IMG_SIZE);
    
    // 1. Initialize RTC First
    if (!rtc.begin()) {
        Serial.println(">> RTC NOT FOUND! Check wiring.");
    }

    // 2. Initialize Modem Serial
    modemSerial.begin(115200, SERIAL_8N1, 16, 17);
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, HIGH); 
    
    // 3. Wait for Modem to respond to AT
    Serial.print(">> Syncing Modem... ");
    unsigned long start = millis();
    bool alive = false;
    while (millis() - start < 15000) {
        if (modem.testAT()) { alive = true; break; }
        delay(500);
    }

    if (!alive) {
        Serial.println("FAIL. Restarting...");
        ESP.restart();
    }
    Serial.println("OK");

    // 4. GPRS Activation
    modem.sendAT("+QIDEACT=1"); 
    modem.sendAT("+QICSGP=1,3,\"jionet\"");
    modem.sendAT("+QIACT=1");
 
    if (modem.waitResponse(10000) == 1) {
        delay(5000); // Wait for signal to register
    
        // --- 7:00 AM MORNING ROLL CALL ONLY ---
        DateTime now = rtc.now();
        if (now.hour() == NIGHT_SLEEP_END) {
            Serial.println(">> Morning window detected. Sending Roll Call SMS...");
            sendStartupSMS(); 
        } else {
            Serial.printf(">> Daytime wake at %02d:%02d. SMS skipped to avoid barrage.\n", now.hour(), now.minute());
        }
    }

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
    }
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("=== HUB ONLINE & LISTENING ===");
}

// --- MAIN LOOP ---
void loop() {
    // --- NIGHT MODE CHECK ---
    DateTime now = rtc.now();
    // Logic: If current hour is >= Start (19) OR < End (7)
    if (now.hour() >= NIGHT_SLEEP_START || now.hour() < NIGHT_SLEEP_END) {
        Serial.printf(">> Night Mode Triggered (%02d:00 - %02d:00).\n", NIGHT_SLEEP_START, NIGHT_SLEEP_END);
        DateTime morningWake;
        if (now.hour() >= NIGHT_SLEEP_START) {
            // Sleep until tomorrow's End Hour
            morningWake = DateTime(now.year(), now.month(), now.day() + 1, NIGHT_SLEEP_END, 0, 0);
        } else {
            // We are already past midnight, sleep until today's End Hour
            morningWake = DateTime(now.year(), now.month(), now.day(), NIGHT_SLEEP_END, 0, 0);
        }
        
        uint64_t sleepSeconds = (morningWake.unixtime() - now.unixtime());
        
        Serial.printf(">> Sleeping for %llu seconds until %02d:00 AM.\n", sleepSeconds, NIGHT_SLEEP_END);
        
        modem.poweroff(); 
        esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
        esp_deep_sleep_start();
    }

    // 1. TELEMETRY GATE
    if (newSensorData && !isModemBusy) {
        newSensorData = false;
        isModemBusy = true;
        incomingData.voltage = readHubBattery(); 
        uploadTelemetry(&incomingData);
        isModemBusy = false;
    }

    // 2. IMAGE GATE
    if (imgSize > 1000 && (millis() - lastImgPacketTime > 4000) && !isModemBusy) {
        isModemBusy = true;
        uploadImage();
        isModemBusy = false;
    } 
    // Noise Cleanup
    else if (imgSize > 0 && (millis() - lastImgPacketTime > 4000)) {
        Serial.printf(">> Noise Cleared (%d bytes)\n", imgSize);
        imgSize = 0; 
    }
}

// --- FUNCTIONS ---
float readHubBattery() {
    int raw = analogRead(HUB_BAT_PIN);
    if (raw == 0) return 0.0;
    float pinVoltage = (raw / 4095.0) * 3.3;
    return pinVoltage * VOLT_FACTOR;
}

void uploadTelemetry(struct_message* data) {
    Serial.println("\n--- [TELEMETRY] ---");
    String url = String(SECRETS_GCP_URL) + "/?token=FARM_SEC&device_id=spoke_" + String(data->id);
    url += "&pct=" + String(data->moisture) + "&bat=" + String(data->voltage);

    Serial.println("URL: " + url);
    modemSerial.print("AT+QHTTPURL=");
    modemSerial.print(url.length());
    modemSerial.print(",80\r\n");

    if (modem.waitResponse(10000, "CONNECT") == 1) {
        modemSerial.print(url);
        if (modem.waitResponse(5000, "OK") == 1) {
            modemSerial.print("AT+QHTTPGET=80\r\n");
            modem.waitResponse(30000, "+QHTTPGET: 0,200");
            Serial.println(">> Success.");
        }
    }
    Serial.println("--- [END] ---");
}

void uploadImage() {
    Serial.println("\n--- [IMAGE PUSH] ---");
    modem.sendAT("+IPR=57600"); 
    modem.waitResponse();
    modemSerial.begin(57600, SERIAL_8N1, 16, 17);
    delay(500);
    modem.sendAT("E0"); modem.waitResponse();

    String url = String(SECRETS_GCP_URL) + "/?token=FARM_SEC&device_id=spoke_2";

    modemSerial.print("AT+QHTTPURL=");
    modemSerial.print(url.length());
    modemSerial.print(",80\r\n");
    
    if (modem.waitResponse(10000, "CONNECT") == 1) {
        modemSerial.print(url);
        modem.waitResponse(5000, "OK");

        uint8_t* startPtr = imgBuffer;
        size_t actualSize = imgSize;
        
        // Find JPEG Header
        for (int i = 0; i < 20 && i < imgSize; i++) {
            if (imgBuffer[i] == 0xFF && imgBuffer[i+1] == 0xD8) {
                startPtr = &imgBuffer[i];
                actualSize = imgSize - i;
                Serial.printf(">> Header at %d\n", i);
                break;
            }
        }

        modemSerial.print("AT+QHTTPPOST=");
        modemSerial.print(actualSize);
        modemSerial.print(",80,80\r\n");

        if (modem.waitResponse(10000, "CONNECT") == 1) {
            size_t written = 0;
            while (written < actualSize) {
                size_t toWrite = (actualSize - written < 128) ? (actualSize - written) : 128;
                modemSerial.write(startPtr + written, toWrite);
                modemSerial.flush(); 
                written += toWrite;
                delay(45); 
                if (written % 2048 == 0) Serial.print(".");
            }
            modem.waitResponse(60000, "+QHTTPPOST: 0,200");
            Serial.println("\n>> Image Success.");
        }
    }

    imgSize = 0;
    modem.sendAT("E1");
    modem.sendAT("+IPR=115200"); 
    modem.waitResponse();
    modemSerial.begin(115200, SERIAL_8N1, 16, 17);
    delay(500);
    Serial.println("--- [END] ---");
}

void IRAM_ATTR OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *data, int len) {
    if (len == sizeof(struct_message)) {
        memcpy(&incomingData, data, sizeof(struct_message));
        newSensorData = true;
    } else {
        if (imgSize + len < MAX_IMG_SIZE) {
            memcpy(imgBuffer + imgSize, data, len);
            imgSize += len;
            lastImgPacketTime = millis();
        }
    }
}

void sendStartupSMS() {
    Serial.println(">> Preparing SMS packet...");
    
    // Read battery first
    float vBat = readHubBattery();
    
    // Use a local string to build the message safely
    String msg = "FarmHub V5.0.2 Online\n";
    
    // Check if RTC is actually running
    DateTime now = rtc.now();
    if (now.year() < 2025) {
        msg += "Time: Not Synced\n";
    } else {
        msg += "Time: " + String(now.hour()) + ":" + String(now.minute()) + "\n";
    }
    
    // Get Signal - TinyGSM returns 99 for unknown
    int csq = modem.getSignalQuality();
    if (csq > 31) csq = 0; 

    msg += "Bat: " + String(vBat, 2) + "V\n";
    msg += "Signal: " + String(csq) + "/31";

    Serial.println(">> Sending to: " + String(SECRETS_ADMIN_PHONE));
    
    // Use the library method directly
    if (modem.sendSMS(SECRETS_ADMIN_PHONE, msg)) {
        Serial.println(">> SMS SUCCESS");
    } else {
        Serial.println(">> SMS FAILED (Check balance/Signal)");
    }
}