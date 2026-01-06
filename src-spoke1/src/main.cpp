#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <RTClib.h>

// --- CONFIGURATION ---
// 1. DESTINATION MAC (Update with your Hub's Actual MAC)
uint8_t broadcastAddress[] = {0xC0, 0xCD, 0xD6, 0x85, 0x18, 0x7C}; 

// 2. SENSOR CALIBRATION (Using your Plastic Jug values)
const int SOIL_PIN = A0;
const int DRY_SOIL = 700;   // Threshold for "Bone Dry" (0%)
const int WET_SOIL = 500;   // Threshold for "Fully Saturated" (100%)

// 3. SLEEP SCHEDULE (24H Format)
const int START_HOUR = 7;     // Wake up at 07:00
const int END_HOUR = 19;      // Sleep at 19:00

// Note: INTERVAL_MINS is removed because we now calculate dynamic snap-to-grid times

RTC_DS3231 rtc;

// Data Structure
typedef struct struct_message {
  int id;          // Node ID
  int moisture;    // Percent Value
  float voltage;   // Battery Voltage (Optional)
} struct_message;

struct_message myData;

// --- FUNCTIONS ---

// 1. Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Packet Status: ");
  Serial.println(sendStatus == 0 ? "Success" : "Fail");
}

// 2. Smart Sensor Reading (With Clipping)
int readSoil() {
  int raw = analogRead(SOIL_PIN);
  Serial.printf("CALIBRATION -> Raw: %d | ", raw); 

  // Zone 1: Too Dry / Air
  if (raw > DRY_SOIL) return 0;
  
  // Zone 2: Too Wet / Water
  if (raw < WET_SOIL) return 100;

  // Zone 3: The Useful Range
  return map(raw, DRY_SOIL, WET_SOIL, 0, 100);
}

// 3. Smart Sleep Calculator (Snap-to-Grid)
long calculateSleepDuration(DateTime now) {
  int currentHour = now.hour();
  long sleepSeconds = 0;

  // --- NIGHT MODE: Sleep untill 7 AM ---
  if (currentHour >= END_HOUR || currentHour < START_HOUR) {
    DateTime tomorrow7am;
    if (currentHour >= END_HOUR) {
       // It's after 19:00, target is tomorrow 7AM
       tomorrow7am = DateTime(now.year(), now.month(), now.day() + 1, START_HOUR, 0, 0);
    } else {
       // It's before 07:00 (e.g. 02:00), target is today 7AM
       tomorrow7am = DateTime(now.year(), now.month(), now.day(), START_HOUR, 0, 0);
    }
    TimeSpan timeDiff = tomorrow7am - now;
    sleepSeconds = timeDiff.totalseconds();
    Serial.printf("Night Time. Hibernating for %ld sec until 07:00.\n", sleepSeconds);
  } 
  
  // --- DAY MODE: Snap to :00 or :30 ---
  else {
    int currentMin = now.minute();
    int currentSec = now.second();
    
    // Logic: If minutes < 30, target is 30. If > 30, target is 60 (next hour 00)
    int nextTargetMin = (currentMin < 30) ? 30 : 60; 
    
    // Calculate minutes remaining
    int minsToSleep = nextTargetMin - currentMin;
    
    // Convert to seconds, subtracting current seconds for precision
    sleepSeconds = (minsToSleep * 60) - currentSec;

    // SAFETY CHECK: 
    // If we are at 10:29:58, sleepSeconds might be 2 seconds. 
    // That's too short. Skip to NEXT slot (30 mins later).
    if (sleepSeconds < 10) {
      sleepSeconds += 1800; // Add 30 mins (1800 seconds)
      Serial.println("Too close to slot boundary! Skipping to next cycle.");
    }

    Serial.printf("Day Time. Current: %02d:%02d:%02d -> Target min: %d\n", currentHour, currentMin, currentSec, nextTargetMin);
    Serial.printf("Aligning to Grid. Sleeping for %ld sec.\n", sleepSeconds);
  }
  
  return sleepSeconds;
}

// --- MAIN SETUP ---
void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1); // SDA=D2, SCL=D1 (Standard NodeMCU I2C)

  // 1. Init RTC
  if (!rtc.begin()) {
    Serial.println("CRITICAL ERROR: RTC Failed! Sleeping 30 mins blindly.");
    ESP.deepSleep(1800e6); // Fallback to raw 30 mins if RTC dies
  }
  
  // UNCOMMENT THIS LINE ONE TIME TO SYNC TIME, THEN RE-UPLOAD WITH IT COMMENTED
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(0, 0, 2, 0));

  DateTime now = rtc.now();
  Serial.printf("\n--- WAKE UP ---\nTime: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());

  // 2. Init WiFi & ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  // 3. Read Sensor
  int percent = readSoil();
  Serial.printf("Moisture: %d%%\n", percent);
  
  // 4. Send Data
  myData.id = 1; 
  myData.moisture = percent;
  // Note: We aren't reading battery voltage yet, sending 0.0 or simulated value
  myData.voltage = 0.0; 
  
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  
  delay(100); // Give ESP-NOW a moment to blast the packet

  // 5. Deep Sleep Calculation
  long sleepSecs = calculateSleepDuration(now);
  
  Serial.println("Going to Deep Sleep...");
  // ULL ensures the calculation is treated as an Unsigned Long Long (64-bit) preventing overflow
  ESP.deepSleep(sleepSecs * 1000000ULL); 
}

void loop() {
  // Loop is empty because Deep Sleep resets the device entirely
}