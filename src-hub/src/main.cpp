/**
 * HUB CODE: "GOLDEN MASTER"
 * - Features: Anti-Conflict Radio Logic, Robust Uploads, Debugged Voltage
 * - Status: FIELD READY
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RTClib.h>
#include <FS.h>
#include <LittleFS.h>
#define SPIFFS LittleFS 

#include "secrets.h" 

// --- 1. CONFIGURATION ---
const int WIFI_CHANNEL = 1;     
const int WINDOW_DURATION = 4;  
const int START_HOUR = 7;       
const int END_HOUR = 19;        
const int IMAGE_TIMEOUT = 10000; 
#define LED_PIN 2               

// VOLTAGE DIVIDER CONFIG (Verified)
#define HUB_BAT_PIN 34 
float voltage_divider_factor = 11.24; 

// --- 2. MODEM SETUP ---
#define TINY_GSM_MODEM_BG96 
#define TINY_GSM_RX_BUFFER 1024 
#include <TinyGsmClient.h>

#define MODEM_PWRKEY 18 
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17

HardwareSerial modemSerial(2);
TinyGsm modem(modemSerial);
RTC_DS3231 rtc;

// --- 3. DATA STRUCTURES ---
typedef struct __attribute__((packed)) struct_message {
  int id;          
  int moisture;    
  float voltage;   
} struct_message;

struct_message incomingSensorData;
volatile bool newSensorDataReceived = false;

// --- RAM BUFFER ---
#define MAX_IMG_SIZE 50000 
uint8_t *imgBuffer;        
volatile size_t imgSize = 0;        
volatile bool isReceivingImage = false;
volatile unsigned long lastImagePacketTime = 0; 
bool imageReadyForSave = false;

// --- 4. HELPER FUNCTIONS ---
void forceModemWake() {
  Serial.println(">> Waking Modem (PWRKEY Sequence)...");
  digitalWrite(MODEM_PWRKEY, LOW); delay(1200);                      
  digitalWrite(MODEM_PWRKEY, HIGH); delay(3000);                      
  modemSerial.print("AT\r\n");      
}

float readHubBattery() {
  long sum = 0;
  for(int i=0; i<10; i++) {
    sum += analogRead(HUB_BAT_PIN);
    delay(2);
  }
  float raw = sum / 10.0;
  return (raw / 4095.0) * 3.3 * voltage_divider_factor;
}

// --- 5. ESP-NOW CALLBACK ---
void IRAM_ATTR OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingDataPtr, int len) {
  // A. SENSOR DATA
  if (len == sizeof(struct_message)) {
    memcpy(&incomingSensorData, incomingDataPtr, sizeof(struct_message));
    newSensorDataReceived = true;
    return;
  }

  // B. CAMERA DATA
  if (len > 100) {
      lastImagePacketTime = millis(); 
      if (!isReceivingImage) {
          imgSize = 0; 
          isReceivingImage = true; 
      }
      if (imgSize + len < MAX_IMG_SIZE) {
         memcpy(imgBuffer + imgSize, incomingDataPtr, len);
         imgSize += len;
         digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 
      }
  }
}

// --- 6. MODEM CONNECTIVITY ---
bool connectToJio() {
  if (modem.isNetworkConnected()) {
      Serial.println(">> Modem is already Online.");
      return true;
  }
  Serial.print(">> Connecting to Tower... ");
  if (!modem.waitForNetwork(15000L)) {
    Serial.println("FAIL (No Signal).");
    return false;
  }
  Serial.println("SUCCESS (Signal Found).");
  
  modem.sendAT("+QIDEACT=1"); modem.waitResponse();
  modem.sendAT("+QICSGP=1,3,\"jionet\""); 
  if (modem.waitResponse() != 1) return false;

  modem.sendAT("+QIACT=1");
  return (modem.waitResponse(10000) == 1);
}

void resetHTTP() {
  modem.sendAT("+QHTTPSTOP");
  modem.waitResponse(2000); 
}

// --- 7. UPLOAD FUNCTIONS ---

// 7a. SENSOR UPLOAD (With BigQuery Fixes)
void uploadSensorData() {
  Serial.println("\n>>> UPLOAD: SENSOR DATA <<<");
  
  if (!connectToJio()) return;

  float hubVolt = readHubBattery();
  int rawADC = analogRead(HUB_BAT_PIN); 
  Serial.printf(">> Hub ADC Raw: %d | Calculated: %.2f V\n", rawADC, hubVolt);

  String urlStr = gcp_url; 
  int httpsIndex = urlStr.indexOf("https://");
  if (httpsIndex != -1) urlStr = urlStr.substring(httpsIndex + 8);
  if (urlStr.endsWith("/")) urlStr = urlStr.substring(0, urlStr.length() - 1);
  String host = urlStr;
  
  // Construct URL with Correct BigQuery Params
  String fullPath = "/?device_id=" + String(incomingSensorData.id) + 
                    "&moisture_pct=" + String(incomingSensorData.moisture) +
                    "&moisture_raw=" + String(incomingSensorData.moisture * 10) + 
                    "&battery_volts=" + String(hubVolt) + 
                    "&token=" + String(API_KEY);

  Serial.print(">> URL: "); Serial.println(fullPath);

  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"requestheader\",0"); modem.waitResponse();

  String full_url = "https://" + host + fullPath;
  modem.sendAT("+QHTTPURL=" + String(full_url.length()) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url);
    modem.waitResponse();
  } else { return; }

  modem.sendAT("+QHTTPGET=80"); 
  if (modem.waitResponse(10000, "OK") == 1) {
     long start = millis();
     while (millis() - start < 10000) {
        while (modem.stream.available()) {
           String line = modem.stream.readStringUntil('\n');
           if (line.indexOf("+QHTTPGET: 0,200") != -1) {
               Serial.println(">> SUCCESS: Sensor Data Uploaded (200 OK)");
               return;
           }
        }
     }
  }
}

// 7b. IMAGE UPLOAD
void saveAndUploadImage() {
  digitalWrite(LED_PIN, LOW); 
  Serial.println("\n>>> PROCESSING IMAGE <<<");
  Serial.printf("Final RAM Size: %d bytes\n", imgSize);

  if (imgSize < 1000) {
      Serial.println("Error: Image too small. Ignoring.");
      return;
  }

  File file = LittleFS.open("/cam_capture.jpg", FILE_WRITE);
  if (!file) { Serial.println("Error: FS Write Failed."); return; }
  file.write(imgBuffer, imgSize);
  file.close();
  Serial.println(">> Saved to LittleFS.");
  imgSize = 0; 

  resetHTTP();
  Serial.println(">> Uploading to GCP...");
  
  file = LittleFS.open("/cam_capture.jpg", FILE_READ);
  size_t fileSize = file.size();
  
  String urlStr = gcp_url; 
  int httpsIndex = urlStr.indexOf("https://");
  if (httpsIndex != -1) urlStr = urlStr.substring(httpsIndex + 8);
  if (urlStr.endsWith("/")) urlStr = urlStr.substring(0, urlStr.length() - 1);
  String host = urlStr;
  String targetPath = "/?token=" + String(API_KEY); 

  String boundary = "------------------------ESP32CamBoundary";
  String bodyHead = "--" + boundary + "\r\n";
  bodyHead += "Content-Disposition: form-data; name=\"image\"; filename=\"cam_capture.jpg\"\r\n";
  bodyHead += "Content-Type: image/jpeg\r\n\r\n";
  String bodyTail = "\r\n--" + boundary + "--\r\n";
  size_t bodyLen = bodyHead.length() + fileSize + bodyTail.length();
  
  String httpHeader = "POST " + targetPath + " HTTP/1.1\r\n";
  httpHeader += "Host: " + host + "\r\n";
  httpHeader += "User-Agent: ESP32_EC200U\r\n";
  httpHeader += "Connection: keep-alive\r\n";
  httpHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  httpHeader += "Content-Length: " + String(bodyLen) + "\r\n\r\n"; 

  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"requestheader\",1"); modem.waitResponse();

  String full_url_for_modem = gcp_url; 
  modem.sendAT("+QHTTPURL=" + String(full_url_for_modem.length()) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url_for_modem);
    modem.waitResponse();
  } else { file.close(); return; }

  size_t totalUploadSize = httpHeader.length() + bodyLen;
  modem.sendAT("+QHTTPPOST=" + String(totalUploadSize) + ",60,60");
  
  if (modem.waitResponse(20000, "CONNECT") == 1) {
    modem.stream.print(httpHeader); delay(50); 
    modem.stream.print(bodyHead);   delay(50);

    uint8_t buf[1024]; 
    while (file.available()) {
      int bytesRead = file.read(buf, sizeof(buf));
      modem.stream.write(buf, bytesRead);
      delay(20); 
    }
    
    modem.stream.print(bodyTail);
    modem.waitResponse(20000); 
  }
  
  file.close();
  modem.sendAT("+QHTTPREAD=80"); 
  long start = millis();
  while (millis() - start < 5000) {
    while (modem.stream.available()) {
      Serial.println(modem.stream.readStringUntil('\n'));
    }
  }
}

void manageSleep(DateTime now) {
  digitalWrite(LED_PIN, LOW); 
  int currentHour = now.hour();
  int currentMin = now.minute();
  int currentSec = now.second();

  if (currentHour >= END_HOUR || currentHour < START_HOUR) {
      Serial.println(">>> NIGHT MODE <<<");
      long secondsInDay = 86400;
      long currentSecondsOfDay = (currentHour * 3600) + (currentMin * 60) + currentSec;
      long targetSecondsOfDay = (START_HOUR * 3600); 
      long secondsUntilMorning;
      if (currentHour >= END_HOUR) secondsUntilMorning = (secondsInDay - currentSecondsOfDay) + targetSecondsOfDay;
      else secondsUntilMorning = targetSecondsOfDay - currentSecondsOfDay;
      
      if (secondsUntilMorning <= 0) secondsUntilMorning = 60; 
      esp_sleep_enable_timer_wakeup(secondsUntilMorning * 1000000ULL);
      esp_deep_sleep_start();
  }

  bool inWindow = false;
  if (currentMin >= 58 || currentMin < (58 + WINDOW_DURATION - 60)) inWindow = true; 
  if (currentMin >= 13 && currentMin < (13 + WINDOW_DURATION)) inWindow = true;
  if (currentMin >= 28 && currentMin < (28 + WINDOW_DURATION)) inWindow = true;
  if (currentMin >= 43 && currentMin < (43 + WINDOW_DURATION)) inWindow = true;

  if (inWindow) return; 

  Serial.println(">>> DAY MODE: Short Nap <<<");
  int nextTargetMin;
  if (currentMin < 13)      nextTargetMin = 13;
  else if (currentMin < 28) nextTargetMin = 28;
  else if (currentMin < 43) nextTargetMin = 43;
  else if (currentMin < 58) nextTargetMin = 58;
  else                      nextTargetMin = 13; 

  int minsToSleep;
  if (nextTargetMin > currentMin) minsToSleep = nextTargetMin - currentMin;
  else minsToSleep = (60 - currentMin) + nextTargetMin;
  
  long sleepSeconds = (minsToSleep * 60) - currentSec;
  if (sleepSeconds <= 0) sleepSeconds = 10; 

  Serial.printf("Napping for %ld seconds...\n", sleepSeconds);
  Serial.flush(); 
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  esp_deep_sleep_start();
}

// --- 8. TIME SYNC FUNCTION (IST FIXED) ---
void syncTimeFromNetwork() {
  Serial.println(">> RTC is Reset! Fetching Network Time...");
  
  if (!connectToJio()) return;
  
  modem.sendAT("+CTZU=1"); modem.waitResponse();
  
  // Request Network Time
  modem.sendAT("+CCLK?");
  if (modem.waitResponse(10000, "+CCLK: \"") == 1) {
    String resp = modem.stream.readStringUntil('\"'); 
    Serial.println(">> Network Time Raw: " + resp);
    
    // Parse UTC Time
    int year = resp.substring(0, 2).toInt() + 2000;
    int month = resp.substring(3, 5).toInt();
    int day = resp.substring(6, 8).toInt();
    int hour = resp.substring(9, 11).toInt();
    int min = resp.substring(12, 14).toInt();
    int sec = resp.substring(15, 17).toInt();
    
    // Create UTC DateTime
    DateTime utcTime = DateTime(year, month, day, hour, min, sec);
    
    // ADD IST OFFSET (+5h 30m)
    // TimeSpan(days, hours, minutes, seconds)
    DateTime istTime = utcTime + TimeSpan(0, 5, 30, 0);
    
    // Update RTC with IST
    rtc.adjust(istTime);
    
    // Print verification
    DateTime now = rtc.now();
    Serial.printf(">> RTC Repaired (IST): %04d/%02d/%02d %02d:%02d\n", 
                  now.year(), now.month(), now.day(), now.hour(), now.minute());
                  
  } else {
    Serial.println(">> Time Sync Failed.");
  }
}

void setup() {
  delay(3000); 

  Serial.begin(115200);
  Wire.begin(21, 22);
  pinMode(HUB_BAT_PIN, INPUT);
  
  pinMode(LED_PIN, OUTPUT); 
  digitalWrite(LED_PIN, LOW); 

  imgBuffer = (uint8_t *)malloc(MAX_IMG_SIZE);
  if (imgBuffer == NULL) while(1); 

  if (!LittleFS.begin(true)) Serial.println("LittleFS Mount Failed!");

  if (!rtc.begin()) Serial.println("RTC Failed!");
  
  DateTime now = rtc.now();
  // --- SAFETY CHECK: IS TIME VALID? ---
  // If year is less than 2025, the RTC has definitely failed/reset. Set to a higher year for occassionlly forcefully setting it through N/W
  if (now.year() < 2025) {
      Serial.println(">> Time Invalid (Pre-2025). Force Syncing...");
      
      // Initialize Modem Logic First to get network
      modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
      pinMode(MODEM_PWRKEY, OUTPUT);
      digitalWrite(MODEM_PWRKEY, HIGH); 
      delay(500);
      
      modem.sendAT("AT"); 
      if (modem.waitResponse(1000) != 1) forceModemWake();

      syncTimeFromNetwork(); // <--- CALL THE FIX
      now = rtc.now(); // Refresh 'now' with correct time
  }
  // ------------------------------------

  int currentHour = now.hour();
  Serial.printf("Time: %02d:%02d\n", currentHour, now.minute());

  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  
  if (esp_now_init() == ESP_OK) {
      esp_now_register_recv_cb(OnDataRecv);
      Serial.println(">> ESP-NOW ACTIVE");
  }

  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); 
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(500);
  
  Serial.println(">> Checking Modem Status...");
  modem.sendAT("AT"); 
  if (modem.waitResponse(1000) != 1) forceModemWake();
  
  connectToJio(); 
  Serial.println("Hub Ready.");
}

void loop() {
  static unsigned long lastCheck = 0;
  static unsigned long lastAlivePrint = 0;

  // 1. SAFETY: RADIO SILENCE
  // If Camera is sending, we DO NOT transmit anything.
  // This solves the "3KB Image" (Interference) issue.
  if (isReceivingImage) {
      if (millis() - lastImagePacketTime > IMAGE_TIMEOUT) { 
          isReceivingImage = false;
          Serial.println(">> Image Stream Ended.");
          imageReadyForSave = true;
      }
      return; // <--- BLOCKING: Crucial!
  }

  // 2. PRIORITY: UPLOAD IMAGE
  if (imageReadyForSave) {
    imageReadyForSave = false;
    saveAndUploadImage();
    return; // Don't do sensor upload immediately
  }

  // 3. PRIORITY: UPLOAD SENSOR
  if (newSensorDataReceived) {
    delay(500); // Buffer check
    if (!isReceivingImage) {
        newSensorDataReceived = false;
        uploadSensorData();
    }
  }

  // 4. SLEEP LOGIC
  if (millis() - lastCheck > 10000) { 
    if (!isReceivingImage && !imageReadyForSave && !newSensorDataReceived) {
        manageSleep(rtc.now());
    }
    lastCheck = millis();
  }

  // 5. HEARTBEAT
  if (millis() - lastAlivePrint > 5000) {
      Serial.println("(Listening for Data...)");
      lastAlivePrint = millis();
  }
}