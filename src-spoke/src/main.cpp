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
// Note: Currently set to 2 mins for testing. Change to 15 for production!
const int INTERVAL_MINS = 30; 

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
  // map(value, low_in, high_in, low_out, high_out)
  return map(raw, DRY_SOIL, WET_SOIL, 0, 100);
}

// 3. Sleep Calculator
long calculateSleepDuration(DateTime now) {
  int currentHour = now.hour();
  long sleepSeconds = 0;

  // NIGHT MODE: Sleep until 7 AM
  if (currentHour >= END_HOUR || currentHour < START_HOUR) {
    DateTime tomorrow7am;
    if (currentHour >= END_HOUR) {
       tomorrow7am = DateTime(now.year(), now.month(), now.day() + 1, START_HOUR, 0, 0);
    } else {
       tomorrow7am = DateTime(now.year(), now.month(), now.day(), START_HOUR, 0, 0);
    }
    TimeSpan timeDiff = tomorrow7am - now;
    sleepSeconds = timeDiff.totalseconds();
    Serial.printf("Night Time. Sleeping for %ld sec until 7AM.\n", sleepSeconds);
  } 
  // DAY MODE: Interval Sleep
  else {
    sleepSeconds = INTERVAL_MINS * 60;
    Serial.printf("Day Time. Sleeping for %d mins.\n", INTERVAL_MINS);
  }
  return sleepSeconds;
}

// --- MAIN SETUP ---
void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1); // SDA=D2, SCL=D1

  // 1. Init RTC
  if (!rtc.begin()) {
    Serial.println("RTC Failed! Defaulting to interval sleep.");
    ESP.deepSleep(INTERVAL_MINS * 60 * 1000000); 
  }
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(0, 0, 2, 0)); // TIME SYNC LINE

  DateTime now = rtc.now();
  Serial.printf("\nTime: %02d:%02d\n", now.hour(), now.minute());

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

  // 3. Read Sensor (Using New Function)
  int percent = readSoil();
  Serial.printf("Moisture: %d%%\n", percent);
  
  // 4. Send Data
  myData.id = 1; 
  myData.moisture = percent;
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  
  delay(100); // Give WiFi time to transmit

  // 5. Deep Sleep
  long sleepSecs = calculateSleepDuration(now);
  ESP.deepSleep(sleepSecs * 1000000); 
}

void loop() {
  // Never reached
}