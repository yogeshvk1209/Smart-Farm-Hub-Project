/**
 * HUB CODE: "VERBOSE EDITION"
 * - Restored full connection logs
 * - Added "Waiting" heartbeat
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

#define HUB_BAT_PIN 34 
float voltage_divider_factor = 11.32; 

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

// --- 5. ESP-NOW CALLBACK ---
void IRAM_ATTR OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingDataPtr, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&incomingSensorData, incomingDataPtr, sizeof(struct_message));
    newSensorDataReceived = true;
    return;
  }

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
  Serial.println("--- CHECKING NETWORK ---");
  
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
  
  Serial.print(">> Config Context (Jionet)... ");
  modem.sendAT("+QIDEACT=1"); modem.waitResponse();
  modem.sendAT("+QICSGP=1,3,\"jionet\""); 
  if (modem.waitResponse() == 1) Serial.println("OK.");
  else Serial.println("FAIL.");

  Serial.print(">> Activating Data... ");
  modem.sendAT("+QIACT=1");
  if (modem.waitResponse(10000) == 1) {
     Serial.println("SUCCESS (IP Assigned).");
     return true;
  } else {
     Serial.println("FAIL.");
     return false;
  }
}

void resetHTTP() {
  modem.sendAT("+QHTTPSTOP");
  modem.waitResponse(2000); 
}

// --- 7. UPLOAD FUNCTIONS ---
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
  
  // URL PARSING
  String urlStr = gcp_url; 
  int httpsIndex = urlStr.indexOf("https://");
  if (httpsIndex != -1) urlStr = urlStr.substring(httpsIndex + 8);
  if (urlStr.endsWith("/")) urlStr = urlStr.substring(0, urlStr.length() - 1);
  String host = urlStr;
  String targetPath = "/?token=" + String(API_KEY); 

  // HEADER PREP
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

  // SSL SETUP
  Serial.print(">> Configuring SSL... ");
  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"requestheader\",1"); modem.waitResponse();
  Serial.println("OK");

  // CONNECT
  String full_url_for_modem = gcp_url; 
  modem.sendAT("+QHTTPURL=" + String(full_url_for_modem.length()) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url_for_modem);
    modem.waitResponse();
  } else { file.close(); return; }

  // SEND
  Serial.println(">> Sending Data Stream...");
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
  
  // RESPONSE
  Serial.println(">> Waiting for Server Response...");
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
  int currentHour = now.hour();
  Serial.printf("Time: %02d:%02d\n", currentHour, now.minute());

  if (currentHour >= END_HOUR || currentHour < START_HOUR) {
    modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, HIGH); 
    delay(100);
    manageSleep(now); 
    return; 
  }

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

  // 1. END OF STREAM
  if (isReceivingImage) {
    if (millis() - lastImagePacketTime > IMAGE_TIMEOUT) { 
      isReceivingImage = false;
      Serial.println(">> Image Stream Ended.");
      imageReadyForSave = true;
    }
  }

  // 2. PROCESS
  if (imageReadyForSave) {
    imageReadyForSave = false;
    saveAndUploadImage();
  }

  // 3. SLEEP CHECK
  if (millis() - lastCheck > 10000) { 
    if (!isReceivingImage && !imageReadyForSave) {
        manageSleep(rtc.now());
    }
    lastCheck = millis();
  }

  // 4. "I AM ALIVE" HEARTBEAT (Every 5 seconds)
  // This shows you the Hub is not stuck, just waiting for the Camera.
  if (millis() - lastAlivePrint > 5000) {
      if (!isReceivingImage) Serial.println("(Waiting for Camera Data...)");
      lastAlivePrint = millis();
  }
}