#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RTClib.h>
#include "secrets.h"

// --- 1. CONFIGURATION ---
const String gcp_url = "https://ingest-farm-data-362443017574.asia-south1.run.app";
const int WIFI_CHANNEL = 1;     // MUST match Spoke
const int WINDOW_DURATION = 5;  // Stay awake for 5 minutes

// API KEY (Must match Python Code)

// SCHEDULE (7 AM to 7 PM)
const int START_HOUR = 7;
const int END_HOUR = 19;

// --- 2. MODEM SETUP ---
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024 
#include <TinyGsmClient.h>

#define MODEM_PWRKEY 18 
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17

HardwareSerial modemSerial(2);
TinyGsm modem(modemSerial);
RTC_DS3231 rtc;

// --- 3. DATA STRUCTURE ---
typedef struct __attribute__((packed)) struct_message {
  int id;          
  int moisture;    // This is receiving % from Spoke, NOT Raw!
  float voltage;   
} struct_message;

struct_message incomingData;
volatile bool newDataReceived = false;

// --- 4. CALLBACKS & HELPERS ---

void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingDataPtr, int len) {
  if (len != sizeof(incomingData)) return;
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  newDataReceived = true; 
}

// --- ROBUST JIO CONNECT (With Signal Wait) ---
bool connectToJio() {
  Serial.println("--- CONNECTING TO JIO ---");
  
  // 1. Wait for Signal (The Critical Fix)
  Serial.print("Waiting for Tower...");
  if (!modem.waitForNetwork(30000L)) {
    Serial.println(" No Signal.");
    return false;
  }
  Serial.println(" Signal Found.");

  if (modem.isNetworkConnected()) {
      Serial.println("Already online.");
      return true;
  }

  modem.sendAT("+QIDEACT=1"); modem.waitResponse();
  modem.sendAT("+QICSGP=1,3,\"jionet\""); 
  if (modem.waitResponse() != 1) return false;

  Serial.print("Activating Data...");
  modem.sendAT("+QIACT=1");
  if (modem.waitResponse(10000) != 1) {
     Serial.println(" Failed.");
     return false;
  }
  return true;
}

void sendManualGET(String params) {
  Serial.println("\n>>> UPLOADING TO CLOUD <<<");

  // HTTP & SSL Config
  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"ciphersuite\",1,0xFFFF"); modem.waitResponse();

  // URL Construction
  String full_url = gcp_url + "/?" + params;
  int urlLen = full_url.length();
  
  Serial.printf("Target Len: %d\n", urlLen);

  // Set URL
  modem.sendAT("+QHTTPURL=" + String(urlLen) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url);
    modem.waitResponse();
  } else {
    Serial.println("Error: URL Setup Failed");
    return;
  }

  // GET Request
  modem.sendAT("+QHTTPGET=80");
  
  // Listen for Response
  long start = millis();
  while (millis() - start < 15000) {
    while (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      
      // Stop immediately if we see Success
      if (line.indexOf("0,200") != -1) {
         Serial.println(">>> SUCCESS: 200 OK <<<");
         return; // <--- HARD EXIT: Stop reading, go to sleep.
      }
      
      // Optional: Print errors if found
      if (line.indexOf("ERROR") != -1) {
          Serial.print("Modem Error: "); Serial.println(line);
      }
    }
    delay(100);
  }
  Serial.println("Timeout: No 200 OK received.");
}

// --- SLEEP MANAGER (Early Riser) ---
void manageSleep(DateTime now) {
  int currentHour = now.hour();
  int currentMin = now.minute();
  
  // A. NIGHT MODE
  if (currentHour >= END_HOUR || currentHour < START_HOUR) {
    if (!(currentHour == (START_HOUR - 1) && currentMin >= 58)) {
      Serial.println("Night Time. Hibernating...");
      int hoursUntilStart = (currentHour >= END_HOUR) ? (24 - currentHour) + START_HOUR : START_HOUR - currentHour;
      long totalSeconds = (hoursUntilStart * 3600) - (currentMin * 60) - now.second() - 120;
      if (totalSeconds <= 0) totalSeconds = 10;
      esp_sleep_enable_timer_wakeup(totalSeconds * 1000000ULL);
      esp_deep_sleep_start();
    }
  }

  // B. DAY MODE (Guard Window :28 and :58)
  bool inWindow = false;
  if (currentMin >= 58 || currentMin <= (WINDOW_DURATION - 2)) inWindow = true; 
  if (currentMin >= 28 && currentMin <= (28 + WINDOW_DURATION)) inWindow = true; 

  if (inWindow) return; // Stay Awake

  // C. NAP MODE
  Serial.println("Window Closed. Napping...");
  int nextTargetMin = (currentMin < 28) ? 28 : 58;
  if (currentMin > 58) nextTargetMin = 28; 
  int minsToSleep = nextTargetMin - currentMin;
  if (minsToSleep < 0) minsToSleep += 60; 
  long sleepSeconds = (minsToSleep * 60) - now.second();
  if (sleepSeconds <= 0) sleepSeconds = 10;

  Serial.printf("Sleeping for %ld seconds...\n", sleepSeconds);
  Serial.flush(); 
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);

  if (!rtc.begin()) Serial.println("RTC Failed!");
  
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); 
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(1000);
  
  Serial.println("Initializing Modem & Network...");
  modem.restart();
  if (connectToJio()) Serial.println("Network Online.");
  else Serial.println("Network Failed.");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Hub Ready. Waiting for Spoke...");
  manageSleep(rtc.now());
}

void loop() {
  static unsigned long lastUploadTime = 0;

  if (newDataReceived) {
    newDataReceived = false;

    // 1. Anti-Duplicate Filter
    if (millis() - lastUploadTime < 10000) {
        Serial.println("Skipping duplicate packet...");
        return; 
    }
    lastUploadTime = millis();

    Serial.printf("Packet: ID %d | Value: %d\n", incomingData.id, incomingData.moisture);

    // 2. MATH FIX: Passthrough only
    // Your spoke is already sending %, so we trust it.
    int raw_val = incomingData.moisture; 
    int pct = incomingData.moisture;

    // 3. Upload with Security Token
    String params = "device_id=" + String(incomingData.id);
    params += "&raw=" + String(raw_val);
    params += "&pct=" + String(pct);
    params += "&bat=" + String(incomingData.voltage, 1);
    params += "&token=" + API_KEY; // <--- Security

    sendManualGET(params);
  }

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) { 
    manageSleep(rtc.now());
    lastCheck = millis();
  }
}
