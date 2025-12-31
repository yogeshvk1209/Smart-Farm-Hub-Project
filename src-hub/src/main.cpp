#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RTClib.h>

// --- CONFIGURATION ---
const int WINDOW_DURATION = 5; // Stay awake for 5 minutes
const int WIFI_CHANNEL = 1;    // MUST match Spoke

// SYNC WITH SPOKE SCHEDULE!
const int START_HOUR = 7;      // Wake up at 7 AM
const int END_HOUR = 19;       // Sleep at 7 PM (19:00)

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
  int currentHour = now.hour();
  int currentMin = now.minute();
  
  // --- 1. NIGHT MODE CHECK ---
  // If it is past bed time OR too early in the morning
  if (currentHour >= END_HOUR || currentHour < START_HOUR) {
    
    // EXCEPTION: If it is 06:59 (the minute before start), treat it as DAY MODE
    // This allows the "Window Logic" below to keep it awake.
    if (currentHour == (START_HOUR - 1) && currentMin >= 59) {
       // Fall through to Day Mode logic
    } else {
      Serial.println("Night Time Detected.");
      
      // Calculate hours until 7:00 AM
      int hoursUntilStart = 0;
      if (currentHour >= END_HOUR) {
        // e.g. 22:00 -> Target 7:00. (24 - 22) + 7 = 9 hours
        hoursUntilStart = (24 - currentHour) + START_HOUR;
      } else {
        // e.g. 05:00 -> Target 7:00. 7 - 5 = 2 hours
        hoursUntilStart = START_HOUR - currentHour;
      }
      
      // We want to wake up at 06:59 (one minute before START_HOUR)
      long totalSeconds = (hoursUntilStart * 3600); 
      
      // Subtract current minutes/seconds to align exactly to the hour
      totalSeconds -= (currentMin * 60) + now.second();
      
      // Subtract 60 seconds to wake up at :59 instead of :00
      totalSeconds -= 60; 
      
      // Safety: If math goes weird or negative, sleep 10 seconds and check again
      if (totalSeconds <= 0) totalSeconds = 10;

      Serial.printf("Hibernating for %ld sec until %02d:59\n", totalSeconds, START_HOUR - 1);
      
      esp_sleep_enable_timer_wakeup(totalSeconds * 1000000ULL);
      esp_deep_sleep_start();
    }
  }

  // --- 2. DAY MODE (Window Logic) ---
  // Logic: Active Window is XX:29-XX:34 OR XX:59-XX:04
  
  bool inWindow = false;
  // Window A: 59 to 04 (Crossing the hour)
  if (currentMin >= 59 || currentMin <= (WINDOW_DURATION - 1)) inWindow = true;
  // Window B: 29 to 34 (Crossing the half-hour)
  if (currentMin >= 29 && currentMin <= (29 + WINDOW_DURATION)) inWindow = true;

  if (inWindow) {
    return; // Stay Awake
  }

  // Calculate Day Sleep (Nap)
  Serial.println("Day Time. Window Closed. Calculating Nap...");
  
  int nextTargetMin = (currentMin < 29) ? 29 : 59;
  if (currentMin > 59) nextTargetMin = 29; 

  int minsToSleep = nextTargetMin - currentMin;
  if (minsToSleep < 0) minsToSleep += 60; 

  long sleepSeconds = (minsToSleep * 60) - now.second();
  
  // Safety clamp
  if (sleepSeconds <= 0) sleepSeconds = 10;

  Serial.printf("Current: %02d:%02d. Napping for %ld sec.\n", currentHour, currentMin, sleepSeconds);
  
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  
  // 1. Init RTC
  if (!rtc.begin()) { Serial.println("RTC Failed!"); }
  
  // ⚠️ UNCOMMENT ONCE TO SET TIME, THEN COMMENT OUT AGAIN
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(0, 0, 2, 0)); // Sync PC time + 2 mins

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
    
    // TODO: Send to EC200U Modem here
  }

  // 2. Continuous Sleep Check (in case window closes while we are awake)
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) { // Check every 10 sec
    manageSleep(rtc.now());
    lastCheck = millis();
  }
}