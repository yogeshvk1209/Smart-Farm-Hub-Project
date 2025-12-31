#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RTClib.h>

// --- CONFIGURATION ---
const int WINDOW_DURATION = 5; // Stay awake for 5 minutes
const int WIFI_CHANNEL = 1;    // MUST match Spoke

RTC_DS3231 rtc;

// --- DATA STRUCTURE ---
typedef struct __attribute__((packed)) struct_message {
  int id;          
  int moisture;    
  float voltage;   
} struct_message;

struct_message incomingData;
volatile bool newDataReceived = false;

// --- FAST CALLBACK ---
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingDataPtr, int len) {
  if (len != sizeof(incomingData)) return;
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  newDataReceived = true; // Set flag and exit immediately
}

// --- SLEEP MANAGER ---
void manageSleep(DateTime now) {
  int currentMin = now.minute();
  
  // Logic: Active Window is XX:29-XX:34 OR XX:59-XX:04
  bool inWindow = false;
  if (currentMin >= 59 || currentMin <= (WINDOW_DURATION - 1)) inWindow = true;
  if (currentMin >= 29 && currentMin <= (29 + WINDOW_DURATION)) inWindow = true;

  if (inWindow) {
    return; // Stay Awake
  }

  // Calculate Sleep
  Serial.println("Window Closed. Calculating Sleep...");
  int nextTargetMin = (currentMin < 29) ? 29 : 59;
  if (currentMin > 59) nextTargetMin = 29; 

  int minsToSleep = nextTargetMin - currentMin;
  if (minsToSleep < 0) minsToSleep += 60; 

  long sleepSeconds = (minsToSleep * 60) - now.second();
  if (sleepSeconds <= 0) sleepSeconds = 10;

  Serial.printf("Current: %02d:%02d. Sleeping for %ld sec.\n", now.hour(), currentMin, sleepSeconds);
  
  // Enable Deep Sleep
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  
  // 1. Init RTC
  if (!rtc.begin()) { Serial.println("RTC Failed!"); }
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Uncomment ONLY if time is wrong

  // 2. Init WiFi & Force Channel
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // 3. Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Error");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("HUB STARTED. Waiting for window or packets...");
  
  // 4. Initial Sleep Check
  manageSleep(rtc.now());
}

void loop() {
  // 1. Handle Data
  if (newDataReceived) {
    newDataReceived = false;
    Serial.println("\n--- PACKET RECEIVED ---");
    Serial.printf("ID: %d | Moisture: %d%%\n", incomingData.id, incomingData.moisture);
    
    // TODO: Send to SIM800L here
  }

  // 2. Continuous Sleep Check (in case window closes while we are awake)
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) { // Check every 10 sec
    manageSleep(rtc.now());
    lastCheck = millis();
  }
}