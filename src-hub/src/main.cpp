/*
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RTClib.h>
#include "secrets.h"

// --- 1. CONFIGURATION ---
//Define you GCP URL in Secrets
//const String gcp_url = "Your Function URL"
const int WIFI_CHANNEL = 1;     // MUST match Spoke
const int WINDOW_DURATION = 5;  // Stay awake for 5 minutes

// API KEY (Must match Python Code)
// Define API keys in Secrets
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
  String full_url = String(gcp_url) + "/?" + params;
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

// B. DAY MODE (Guard Windows: :58, :13, :28, :43)
  bool inWindow = false;
  
  // 1. The Wrap-Around Window (Starts :58, ends :03 approx)
  if (currentMin >= 58 || currentMin <= (WINDOW_DURATION - 2)) inWindow = true; 
  
  // 2. The New Windows (:13, :28, :43)
  if (currentMin >= 13 && currentMin <= (13 + WINDOW_DURATION)) inWindow = true; 
  if (currentMin >= 28 && currentMin <= (28 + WINDOW_DURATION)) inWindow = true; 
  if (currentMin >= 43 && currentMin <= (43 + WINDOW_DURATION)) inWindow = true; 

  if (inWindow) {
    Serial.println("In Active Window. Working...");
    return; // Stay Awake
  }

  // C. NAP MODE (Find next target: 13, 28, 43, 58)
  Serial.println("Window Closed. Napping...");
  
  int nextTargetMin;

  if (currentMin < 13)      nextTargetMin = 13;
  else if (currentMin < 28) nextTargetMin = 28;
  else if (currentMin < 43) nextTargetMin = 43;
  else if (currentMin < 58) nextTargetMin = 58;
  else                      nextTargetMin = 13; // Wrapped to next hour

  // Calculate Duration
  int minsToSleep = nextTargetMin - currentMin;
  if (minsToSleep < 0) minsToSleep += 60; // Handle wrap (e.g., 59 -> 13)
  
  long sleepSeconds = (minsToSleep * 60) - now.second();
  
  // Safety buffer: never sleep less than 10s (avoids boot loops)
  if (sleepSeconds <= 0) sleepSeconds = 10;

  Serial.printf("Next wake at :%02d. Sleeping for %ld seconds...\n", nextTargetMin, sleepSeconds);
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
    params += "&token=" + String(API_KEY); // <--- Security

    sendManualGET(params);
  }

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) { 
    manageSleep(rtc.now());
    lastCheck = millis();
  }
}
*/
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPIFFS.h>      // Added for Image Storage
#include "secrets.h"

// --- 1. CONFIGURATION ---
// const String gcp_url = "Your Function URL"; // Defined in secrets.h
const int WIFI_CHANNEL = 1;     // MUST match Spoke
const int WINDOW_DURATION = 5;  // Stay awake for 5 minutes

// API KEY (Must match Python Code)
// const String API_KEY = "YourKey"; // Defined in secrets.h

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

// --- 4. CALLBACKS (Core 3.0+ Compatible) ---

// New Signature: (info, data, len)
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingDataPtr, int len) {
  
  // CASE 1: SENSOR DATA (Spoke 1)
  if (len == sizeof(struct_message)) {
    memcpy(&incomingSensorData, incomingDataPtr, sizeof(struct_message));
    newSensorDataReceived = true;
    
    // Optional: Log who sent it
    // Serial.printf("Data from: %02X:%02X:%02X:%02X:%02X:%02X\n", 
    //               info->src_addr[0], info->src_addr[1], info->src_addr[2], 
    //               info->src_addr[3], info->src_addr[4], info->src_addr[5]);
    return;
  }

  // CASE 2: CAMERA CHUNKS (Spoke 2)
  if (len > 50) { 
    lastImagePacketTime = millis();
    
    // If this is the FIRST chunk of a new image, open the file
    if (!isReceivingImage) {
      Serial.println(">> START: Receiving Image Stream...");
      isReceivingImage = true;
      imageFile = SPIFFS.open("/cam_capture.jpg", FILE_WRITE); 
      if (!imageFile) Serial.println("!! ERROR: Could not open SPIFFS file");
    }
    
    // Append Data
    if (imageFile) {
      imageFile.write(incomingDataPtr, len);
    }
  }
}

// --- 5. MODEM & UPLOAD FUNCTIONS ---

bool connectToJio() {
  Serial.println("--- CONNECTING TO JIO ---");
  
  // Note: While this waits, the ESP-NOW radio is ALREADY ON (thanks to Setup fix)
  // so it can ACK the camera even while we wait here.
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

void uploadSensorData() {
  Serial.println("\n>>> UPLOADING SENSOR DATA <<<");

  // HTTP & SSL Config
  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"ciphersuite\",1,0xFFFF"); modem.waitResponse();

  // Prepare Params
  int raw_val = incomingSensorData.moisture; 
  int pct = incomingSensorData.moisture; // Passthrough

  String params = "device_id=" + String(incomingSensorData.id);
  params += "&raw=" + String(raw_val);
  params += "&pct=" + String(pct);
  params += "&bat=" + String(incomingSensorData.voltage, 1);
  params += "&token=" + String(API_KEY); 

  String full_url = String(gcp_url) + "/?" + params;
  int urlLen = full_url.length();
  
  // Send URL
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
  
  // Simple Response Wait
  long start = millis();
  while (millis() - start < 15000) {
    while (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      if (line.indexOf("0,200") != -1) {
         Serial.println(">>> SUCCESS: 200 OK <<<");
         return; 
      }
    }
    delay(100);
  }
  Serial.println("Timeout: No 200 OK received.");
}

// ==========================================
//  REAL IMAGE UPLOAD (EC200U SPECIFIC)
//  Uses Raw Header Mode for Multipart
// ==========================================
void uploadImage() {
  Serial.println("\n>>> UPLOAD: Starting EC200U Image Sequence <<<");

  File file = SPIFFS.open("/cam_capture.jpg", FILE_READ);
  if (!file) {
    Serial.println("Error: No image file found.");
    return;
  }
  
  size_t fileSize = file.size();
  if (fileSize == 0) {
    file.close();
    return;
  }

  // --- 1. PARSE URL & HOST ---
  // We need the plain Hostname (e.g. "xyz.run.app") for the HTTP Headers
  String urlStr = gcp_url; 
  // Remove "https://"
  int httpsIndex = urlStr.indexOf("https://");
  if (httpsIndex != -1) {
    urlStr = urlStr.substring(httpsIndex + 8);
  }
  // Remove trailing slash if present
  if (urlStr.endsWith("/")) {
    urlStr = urlStr.substring(0, urlStr.length() - 1);
  }
  String host = urlStr;
  
  // Construct the Target Path (including the Secret Token)
  String targetPath = "/?token=" + String(API_KEY);
  
  Serial.println("Host: " + host);
  Serial.println("Path: " + targetPath);

  // --- 2. PREPARE MULTIPART BODY ---
  String boundary = "------------------------ESP32CamBoundary";
  
  // The Body "Head" (Before the file data)
  String bodyHead = "--" + boundary + "\r\n";
  bodyHead += "Content-Disposition: form-data; name=\"image\"; filename=\"cam_capture.jpg\"\r\n";
  bodyHead += "Content-Type: image/jpeg\r\n\r\n";
  
  // The Body "Tail" (After the file data)
  String bodyTail = "\r\n--" + boundary + "--\r\n";

  // Total Payload Size (Body Only)
  size_t bodyLen = bodyHead.length() + fileSize + bodyTail.length();

  // --- 3. PREPARE RAW HTTP HEADERS ---
  // Since we use "requestheader=1", we must type the FULL HTTP Request
  String httpHeader = "POST " + targetPath + " HTTP/1.1\r\n";
  httpHeader += "Host: " + host + "\r\n";
  httpHeader += "User-Agent: ESP32_EC200U\r\n";
  httpHeader += "Connection: keep-alive\r\n";
  httpHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  httpHeader += "Content-Length: " + String(bodyLen) + "\r\n\r\n"; 
  // Note: The double \r\n\r\n marks the end of headers

  // --- 4. CALCULATE TOTAL UPLOAD SIZE ---
  size_t totalUploadSize = httpHeader.length() + bodyLen;
  Serial.printf("Total Upload Bytes: %d\n", totalUploadSize);

  // --- 5. CONFIGURE EC200U HTTP ---
  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"ciphersuite\",1,0xFFFF"); modem.waitResponse();
  
  // CRITICAL FOR EC200U MULTIPART: Enable Raw Headers
  modem.sendAT("+QHTTPCFG=\"requestheader\",1"); 
  modem.waitResponse();

  // --- 6. SET URL (For DNS & Connection) ---
  // Even in raw mode, we still set the base URL for the modem to resolve IP
  String full_url_for_modem = gcp_url; 
  modem.sendAT("+QHTTPURL=" + String(full_url_for_modem.length()) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url_for_modem);
    modem.waitResponse();
  } else {
    Serial.println("Error: URL Set Failed"); file.close(); return;
  }

  // --- 7. START POST (RAW STREAM) ---
  Serial.println(">> Starting Data Stream...");
  modem.sendAT("+QHTTPPOST=" + String(totalUploadSize) + ",60,60");
  
  if (modem.waitResponse(20000, "CONNECT") == 1) {
    // A. Send The HTTP Headers
    modem.stream.print(httpHeader);
    
    // B. Send Body Head
    modem.stream.print(bodyHead);
    
    // C. Send Image Data (Chunks)
    uint8_t buf[1024]; // Larger buffer for speed
    while (file.available()) {
      int bytesRead = file.read(buf, sizeof(buf));
      modem.stream.write(buf, bytesRead);
      // Tiny delay not usually needed for EC200U, but safe
      delay(5); 
    }
    
    // D. Send Body Tail
    modem.stream.print(bodyTail);
    
    Serial.println(">> Data Sent. Waiting for server...");
    modem.waitResponse(20000); // Wait for OK
  } else {
    Serial.println("Error: Connect Failed"); file.close(); return;
  }
  
  file.close();

  // --- 8. READ RESPONSE ---
  Serial.println(">> Reading Response...");
  modem.sendAT("+QHTTPREAD=80");
  long start = millis();
  while (millis() - start < 10000) {
    while (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      Serial.println(line); 
      if (line.indexOf("200") != -1 || line.indexOf("Image Uploaded") != -1) {
         Serial.println(">>> UPLOAD SUCCESS! <<<");
         return; 
      }
    }
    delay(100);
  }
}

// --- 6. SLEEP MANAGER ---
void manageSleep(DateTime now) {
  return;
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

  // B. DAY MODE (Guard Window)
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

// --- 7. SETUP (THE CRITICAL PART) ---
void setup() {
  Serial.begin(115200);

  // ====================================================
  // STEP 1: ENABLE WIFI RADIO IMMEDIATELY (THE FIX)
  // This ensures the Hub can ACK the camera even if 
  // the Modem code below takes 60 seconds to connect.
  // ====================================================
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  
  if (esp_now_init() == ESP_OK) {
      esp_now_register_recv_cb(OnDataRecv);
      Serial.println(">> ESP-NOW ACTIVE: Listening for Camera/Sensors...");
  } else {
      Serial.println("!! ESP-NOW INIT FAILED !!");
  }
  // ====================================================

  // STEP 2: MOUNT FILESYSTEM (For Image)
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed!");
  }

  // STEP 3: HARDWARE & RTC
  if (!rtc.begin()) Serial.println("RTC Failed!");
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); 
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(1000);
  
  // STEP 4: CONNECT TO JIO (Blocking/Slow)
  Serial.println("Initializing Modem...");
  modem.restart();
  connectToJio(); 
  // ^^^ While this is running, WiFi radio is ON and handling ACKs.

  Serial.println("Hub Setup Complete.");
  // manageSleep(rtc.now());
  Serial.println(">> TEST MODE: Staying Awake Forever...");
}

void loop() {
  static unsigned long lastCheck = 0;

  // --- A. HANDLE SENSOR DATA ---
  if (newSensorDataReceived) {
    newSensorDataReceived = false;
    Serial.printf("Sensor Packet: ID %d | Value: %d\n", incomingSensorData.id, incomingSensorData.moisture);
    uploadSensorData();
  }

  // --- B. HANDLE IMAGE COMPLETION ---
  if (isReceivingImage) {
    // If no new chunks received for 2 seconds, assume image is done
    if (millis() - lastImagePacketTime > IMAGE_TIMEOUT) {
      isReceivingImage = false;
      if (imageFile) imageFile.close(); 
      
      Serial.println(">> Image Stream Ended.");
      imageReadyForUpload = true;
    }
  }

  // --- C. TRIGGER IMAGE PROCESSING ---
  if (imageReadyForUpload) {
    imageReadyForUpload = false;
    uploadImage(); // Currently just logs size
  }

  // --- D. SLEEP CHECK ---
  if (millis() - lastCheck > 10000) { 
    // Only sleep if NOT currently receiving an image
    if (!isReceivingImage && !imageReadyForUpload) {
        manageSleep(rtc.now());
    }
    lastCheck = millis();
  }
}