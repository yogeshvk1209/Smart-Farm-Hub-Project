#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <RTClib.h>

// --- CONFIGURATION ---
// 1. DESTINATION MAC (Update with your Hub's Actual MAC)
//
uint8_t broadcastAddress[] = {0xC0, 0xCD, 0xD6, 0x85, 0x18, 0x7C}; 

// 2. HARDWARE PINS
const int SOIL_PIN = A0;
const int SENSOR_PWR_PIN = 14; // Pin D5 on NodeMCU/D1 Mini

// 3. CALIBRATION (Update with your specific dry/wet values)
const int DRY_SOIL = 700;   // Threshold for "Bone Dry" (0%)
const int WET_SOIL = 300;   // Threshold for "Fully Saturated" (100%)

// 4. SLEEP SCHEDULE (24H Format)
const int START_HOUR = 7;     // Wake up at 07:00
const int END_HOUR = 19;      // Sleep at 19:00

// --- OBJECTS & STRUCTS ---
RTC_DS3231 rtc;

// FIX: Use packed attribute to match ESP32 Hub struct exactly
typedef struct __attribute__((packed)) struct_message {
  int id;          // Node ID
  int moisture;    // Percent Value
  float voltage;   // Battery Voltage
} struct_message;

struct_message myData;

// --- FUNCTIONS ---

// 1. Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("\n>> Packet Status: ");
  Serial.println(sendStatus == 0 ? "Success" : "Fail");
}

// 2. Smart Sensor Reading (Power Gated via D5)
int readSoil() {
  Serial.println(">> Reading Sensor...");
  
  // A. Power ON (Legacy support if using D5, harmless if using 3.3V Direct)
  pinMode(SENSOR_PWR_PIN, OUTPUT);
  digitalWrite(SENSOR_PWR_PIN, HIGH);
  delay(800); // Wait for sensor to stabilize

  // B. Read
  int raw = analogRead(SOIL_PIN);
  Serial.printf("   [Raw Value: %d] ", raw); 

  // C. Power OFF (Save Battery!)
  digitalWrite(SENSOR_PWR_PIN, LOW);
  pinMode(SENSOR_PWR_PIN, INPUT); 

  // D. Map & Clip
  if (raw > DRY_SOIL) return 0;
  if (raw < WET_SOIL) return 100;
  return map(raw, DRY_SOIL, WET_SOIL, 0, 100);
}

// 3. Smart Sleep Calculator (Targeting xx:13 and xx:43)
long calculateSleepDuration(DateTime now) {
  int currentHour = now.hour();
  long sleepSeconds = 0;

  // --- NIGHT MODE: Sleep until 07:00 AM ---
  if (currentHour >= END_HOUR || currentHour < START_HOUR) {
    DateTime tomorrow7am;
    if (currentHour >= END_HOUR) {
       tomorrow7am = DateTime(now.year(), now.month(), now.day() + 1, START_HOUR, 0, 0);
    } else {
       tomorrow7am = DateTime(now.year(), now.month(), now.day(), START_HOUR, 0, 0);
    }
    TimeSpan timeDiff = tomorrow7am - now;
    sleepSeconds = timeDiff.totalseconds();
    Serial.printf("\n>> Night Mode. Sleeping %ld sec until 07:00.\n", sleepSeconds);
  } 
  
  // --- DAY MODE: Target xx:28 or xx:58 marks ---
  else {
    int currentMin = now.minute();
    int currentSec = now.second();
    int targetMin;

    if (currentMin < 28) {
        targetMin = 28;
    } else if (currentMin < 58) {
        targetMin = 58;
    } else {
        targetMin = 88; // Target :28 of the next hour
    }
    
    int minutesToSleep = targetMin - currentMin;
    sleepSeconds = (minutesToSleep * 60) - currentSec;

    // Safety: If too close, skip to next 30-min slot
    if (sleepSeconds < 10) {
      sleepSeconds += 1800;
    }

    Serial.printf("\n>> Day Mode. Now: %02d:%02d -> Target: %d", 
                  currentHour, currentMin, (targetMin > 60 ? targetMin - 60 : targetMin));
    Serial.printf("\n>> Sleeping for %ld sec.\n", sleepSeconds);
  }
  return sleepSeconds;
}

// 4. ALIGN TO TIME SLOT (The "Hold Your Horses" Logic)
void alignToSlot(DateTime now) {
  int currentSec = now.second();
  int currentMin = now.minute();
  
  // --- THE FIX: SHIFT TO :25 SECONDS ---
  // Old Value: 5 (Collision with Camera)
  // New Value: 25 (Safe Zone after Camera finishes)
  int targetSec = 25; 
  // -------------------------------------
  
  long waitMillis = 0;

  // Case 1: Early Wakeup (e.g. 15:12:59)
  if (currentSec > 45) {
     Serial.printf(">> Early Wakeup. Waiting for :%d...\n", targetSec);
     waitMillis = ((60 - currentSec) + targetSec) * 1000;
  }
  
  // Case 2: On Time but before target (e.g. 15:13:01)
  else if (currentSec < targetSec) {
     Serial.printf(">> Waiting for clean air (:25)...\n");
     waitMillis = (targetSec - currentSec) * 1000;
  }
  
  if (waitMillis > 0) {
    Serial.printf(">> Holding for %ld ms to avoid Camera collision...\n", waitMillis);
    delay(waitMillis);
  }
}

// --- MAIN SETUP ---
void setup() {
  // Init Serial FIRST so we see boot messages
  Serial.begin(115200);
  Serial.println("\n\n=== SPOKE 1 BOOT ===");

  // Init I2C for RTC
  Wire.begin(D2, D1); // SDA=D2, SCL=D1

  // Init RTC
  if (!rtc.begin()) {
    Serial.println("CRITICAL: RTC Missing! Sleeping 30 mins blindly.");
    ESP.deepSleep(1800e6); 
  }
  
  // RTC SYNC (Uncomment ONLY if time is wrong, upload, then comment out & re-upload)
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  DateTime now = rtc.now();
  Serial.printf("Wake Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());

  // 1. ALIGN TIME (Wait for Hub to be ready)
  alignToSlot(now);

  // 2. NOW we initialize WiFi (Hub is definitely awake now)
  WiFi.mode(WIFI_STA);
  // --- FIX: FORCE CHANNEL 1 ---
  wifi_set_channel(1); 
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed");
    ESP.deepSleep(60e6); // Sleep 1 min and retry
  }
  
  // Register Peer
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  // --- JOB: MEASURE & SEND ---
  int percent = readSoil();
  
  myData.id = 1; 
  myData.moisture = percent;
  myData.voltage = 0.0; // Battery reading disabled for Spoke 1 (A0 used by sensor)
  
  Serial.println(">> Sending Packet...");
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  
  delay(200); // Allow time for packet to leave radio

  // --- JOB: SLEEP ---
  // Re-read time because we might have waited in alignToSlot
  now = rtc.now();
  long sleepSecs = calculateSleepDuration(now);
  
  // Safety: If we just sent at 14:43:05, ensure we don't calculate "0" seconds.
  if (sleepSecs < 10) sleepSecs = 10; 
  
  Serial.printf(">> Done. Sleeping %ld sec.\n", sleepSecs);
  Serial.println(">> Goodnight.");
  
  // Convert to Microseconds for Deep Sleep
  ESP.deepSleep(sleepSecs * 1000000ULL); 
}

void loop() {
  // Empty. Deep Sleep restarts setup().
}