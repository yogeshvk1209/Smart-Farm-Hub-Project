#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPIFFS.h>      
#include "secrets.h" 
// secrets.h must contain: const char* API_KEY, const char* gcp_url

// --- 1. CONFIGURATION ---
const int WIFI_CHANNEL = 1;     // MUST match Spoke
const int WINDOW_DURATION = 4;  // Stay awake for 4 mins (e.g., 13 to 17)
const int START_HOUR = 7;       // 7 AM
const int END_HOUR = 19;        // 7 PM

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

// --- 3. DATA STRUCTURES ---

// A. SENSOR DATA (Spoke 1)
typedef struct __attribute__((packed)) struct_message {
  int id;          
  int moisture;    // Receiving % from Spoke
  float voltage;   
} struct_message;

struct_message incomingSensorData;
volatile bool newSensorDataReceived = false;

// B. CAMERA DATA (Spoke 2)
File imageFile;
volatile bool isReceivingImage = false;
unsigned long lastImagePacketTime = 0;
const int IMAGE_TIMEOUT = 2000; // If no packets for 2s, assume image done
bool imageReadyForUpload = false;

// --- 4. CALLBACKS ---

void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingDataPtr, int len) {
  
  // CASE 1: SENSOR DATA (Fixed Size Struct)
  if (len == sizeof(struct_message)) {
    memcpy(&incomingSensorData, incomingDataPtr, sizeof(struct_message));
    newSensorDataReceived = true;
    return;
  }

  // CASE 2: CAMERA CHUNKS (Variable Large Size)
  if (len > sizeof(struct_message) + 10) { 
    lastImagePacketTime = millis();
    
    // First chunk? Open/Create file.
    if (!isReceivingImage) {
      Serial.println(">> START: Receiving Image Stream...");
      isReceivingImage = true;
      imageFile = SPIFFS.open("/cam_capture.jpg", FILE_WRITE); 
      if (!imageFile) Serial.println("!! ERROR: Could not open SPIFFS");
    }
    
    // Append Data
    if (imageFile) {
      imageFile.write(incomingDataPtr, len);
    }
  }
}

// --- 5. MODEM FUNCTIONS ---

bool connectToJio() {
  Serial.println("--- CONNECTING TO JIO ---");
  Serial.print("Waiting for Tower...");
  if (!modem.waitForNetwork(15000L)) {
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

void resetHTTP() {
  Serial.println(">> Cleaning HTTP State...");
  modem.sendAT("+QHTTPSTOP");
  modem.waitResponse(2000); 
}

// --- UPLOAD 1: SENSOR DATA (GET REQUEST) ---
void uploadSensorData() {
  resetHTTP();
  Serial.println("\n>>> UPLOADING SENSOR DATA <<<");
  delay(1000); 

  modem.sendAT("+QHTTPCFG=\"requestheader\",0"); 
  modem.waitResponse();

  // 1. Prepare Params
  int raw_val = incomingSensorData.moisture; 
  int pct = incomingSensorData.moisture; 

  String params = "device_id=" + String(incomingSensorData.id);
  params += "&raw=" + String(raw_val);
  params += "&pct=" + String(pct);
  params += "&bat=" + String(incomingSensorData.voltage, 1);
  params += "&token=" + String(API_KEY); 

  String baseUrl = gcp_url;
  if (baseUrl.endsWith("/")) {
    baseUrl = baseUrl.substring(0, baseUrl.length() - 1);
  }
  String full_url = baseUrl + "/?" + params;
  
  Serial.print("Target URL: ");
  Serial.println(full_url); 

  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();

  modem.sendAT("+QHTTPURL=" + String(full_url.length()) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url);
    modem.waitResponse();
  } else {
    Serial.println("Error: URL Setup Failed");
    return;
  }

  Serial.println(">> Sending GET Request...");
  modem.sendAT("+QHTTPGET=80");
  
  long start = millis();
  while (millis() - start < 15000) {
    while (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      line.trim();
      
      if (line.length() > 0) {
        Serial.print("[Modem]: ");
        Serial.println(line); 

        if (line.indexOf("0,200") != -1) {
           Serial.println(">>> SENSOR SUCCESS: 200 OK <<<");
           return; 
        }
        if (line.indexOf("ERROR") != -1) return;
      }
    }
  }
}

// --- UPLOAD 2: IMAGE DATA (POST MULTIPART) ---
void uploadImage() {
  resetHTTP();
  Serial.println("\n>>> UPLOAD: Starting Image Sequence <<<");

  File file = SPIFFS.open("/cam_capture.jpg", FILE_READ);
  if (!file || file.size() == 0) {
    Serial.println("Error: Invalid image file.");
    if (file) file.close();
    return;
  }
  size_t fileSize = file.size();
  Serial.printf("File Size: %d bytes\n", fileSize);

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
  } else {
    file.close(); return;
  }

  Serial.println(">> Sending Data Stream...");
  size_t totalUploadSize = httpHeader.length() + bodyLen;
  modem.sendAT("+QHTTPPOST=" + String(totalUploadSize) + ",60,60");
  
  if (modem.waitResponse(20000, "CONNECT") == 1) {
    modem.stream.print(httpHeader);
    delay(50); 
    modem.stream.print(bodyHead);
    delay(50);

    uint8_t buf[1024]; 
    while (file.available()) {
      int bytesRead = file.read(buf, sizeof(buf));
      modem.stream.write(buf, bytesRead);
      delay(20); 
    }
    
    modem.stream.print(bodyTail);
    Serial.println(">> Stream Complete.");
    modem.waitResponse(20000); 
  } else {
    file.close(); return;
  }
  
  file.close();
  Serial.println(">> Waiting for Cloud...");
  delay(5000); 

  modem.sendAT("+QHTTPREAD=80"); 
  long start = millis();
  while (millis() - start < 10000) {
    while (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      line.trim(); 
      if (line.length() > 0) {
        Serial.print("[Cloud]: ");
        Serial.println(line); 
        if (line.indexOf("success") != -1) Serial.println(">>> UPLOAD CONFIRMED! <<<");
        if (line == "OK" || line == "ERROR") return;
      }
    }
  }
}

// --- 6. SLEEP MANAGER (Corrected for RTC_DS3231) ---
void manageSleep(DateTime now) {
  int currentHour = now.hour();
  int currentMin = now.minute();
  int currentSec = now.second();

  // 1. NIGHT MODE (7 PM to 6:58 AM)
  if (currentHour >= END_HOUR || (currentHour < START_HOUR && !(currentHour == 6 && currentMin >= 58))) {
      Serial.println("Night Time. Hibernating until 06:58...");
      
      long secondsUntilMorning;
      long secondsInDay = 86400;
      long currentSecondsOfDay = (currentHour * 3600) + (currentMin * 60) + currentSec;
      long targetSecondsOfDay = (6 * 3600) + (58 * 60); // 06:58:00
      
      if (currentHour >= END_HOUR) {
        secondsUntilMorning = (secondsInDay - currentSecondsOfDay) + targetSecondsOfDay;
      } else {
        secondsUntilMorning = targetSecondsOfDay - currentSecondsOfDay;
      }

      esp_sleep_enable_timer_wakeup(secondsUntilMorning * 1000000ULL);
      esp_deep_sleep_start();
  }

  // 2. ACTIVE WINDOW CHECK
  // Windows: xx:58, xx:13, xx:28, xx:43
  bool inWindow = false;

  if (currentMin >= 58 || currentMin < (58 + WINDOW_DURATION - 60)) inWindow = true; 
  if (currentMin >= 13 && currentMin < (13 + WINDOW_DURATION)) inWindow = true;
  if (currentMin >= 28 && currentMin < (28 + WINDOW_DURATION)) inWindow = true;
  if (currentMin >= 43 && currentMin < (43 + WINDOW_DURATION)) inWindow = true;

  if (inWindow) return; // Stay Awake!

  // 3. NAP MODE
  Serial.println("Window Closed. Napping...");
  
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

  Serial.printf("Next wake at :%02d. Sleeping for %ld seconds...\n", nextTargetMin, sleepSeconds);
  Serial.flush(); 
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);

  // STEP 1: ESP-NOW INIT
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  
  if (esp_now_init() == ESP_OK) {
      esp_now_register_recv_cb(OnDataRecv);
      Serial.println(">> ESP-NOW ACTIVE");
  } else {
      Serial.println("!! ESP-NOW FAILED !!");
  }

  // STEP 2: FILESYSTEM & RTC
  if (!SPIFFS.begin(true)) Serial.println("SPIFFS Mount Failed!");
  if (!rtc.begin()) Serial.println("RTC Failed!");
  // Optional: Uncomment below to set time once, then comment out and re-upload
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // STEP 3: MODEM SETUP
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); 
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(1000);
  
  Serial.println("Initializing Modem...");
  modem.restart();
  connectToJio(); 

  Serial.println("Hub Ready.");
}

void loop() {
  static unsigned long lastCheck = 0;

  // 1. Handle Sensor Data
  if (newSensorDataReceived) {
    newSensorDataReceived = false;
    Serial.printf("Sensor Packet: ID %d | Value: %d\n", incomingSensorData.id, incomingSensorData.moisture);
    uploadSensorData();
  }

  // 2. Handle Image Stream End
  if (isReceivingImage) {
    if (millis() - lastImagePacketTime > IMAGE_TIMEOUT) {
      isReceivingImage = false;
      if (imageFile) imageFile.close(); 
      Serial.println(">> Image Stream Ended.");
      imageReadyForUpload = true;
    }
  }

  // 3. Handle Image Upload
  if (imageReadyForUpload) {
    imageReadyForUpload = false;
    uploadImage();
  }

  // 4. Sleep Logic 
  if (millis() - lastCheck > 10000) { 
    if (!isReceivingImage && !imageReadyForUpload) {
        manageSleep(rtc.now());
    }
    lastCheck = millis();
  }
}