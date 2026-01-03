#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPIFFS.h>      // Required for buffering image
#include "secrets.h"

// --- 1. CONFIGURATION ---
const int WIFI_CHANNEL = 1;     // MUST match Spoke
const int WINDOW_DURATION = 5;  // Stay awake for 5 minutes

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

// --- 4. CALLBACKS ---

void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingDataPtr, int len) {
  
  // CASE 1: SENSOR DATA (Fixed Size Struct)
  if (len == sizeof(struct_message)) {
    memcpy(&incomingSensorData, incomingDataPtr, sizeof(struct_message));
    newSensorDataReceived = true;
    return;
  }

  // CASE 2: CAMERA CHUNKS (Variable Large Size)
  // We assume anything larger than the struct is an image chunk
  if (len > sizeof(struct_message) + 10) { 
    lastImagePacketTime = millis();
    
    // First chunk? Open/Create file.
    if (!isReceivingImage) {
      Serial.println(">> START: Receiving Image Stream...");
      isReceivingImage = true;
      // "w" mode truncates existing file (starts fresh)
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
  // Reduced timeout slightly to 15s to not block too long if bad signal
  if (!modem.waitForNetwork(15000L)) {
    Serial.println(" No Signal.");
    return false;
  }
  Serial.println(" Signal Found.");

  if (modem.isNetworkConnected()) {
      Serial.println("Already online.");
      return true;
  }

  // Force re-connect sequence
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

// Function to Reset the HTTP as Modem keeps getting stuck after some requests
void resetHTTP() {
  Serial.println(">> Cleaning HTTP State...");
  // Stop any pending HTTP sessions
  modem.sendAT("+QHTTPSTOP");
  modem.waitResponse(2000); 
  // Release context (Optional but safe)
  // modem.sendAT("+QHTTPCFG=\"contextid\",1"); 
}

// --- UPLOAD 1: SENSOR DATA (GET REQUEST) ---
void uploadSensorData() {
  resetHTTP();
  Serial.println("\n>>> UPLOADING SENSOR DATA <<<");
  delay(1000); 

  // --- FIX: DISABLE RAW HEADERS (Turn off the "Image Mode") ---
  modem.sendAT("+QHTTPCFG=\"requestheader\",0"); 
  modem.waitResponse();
  // -----------------------------------------------------------

  // 1. Prepare Params
  int raw_val = incomingSensorData.moisture; 
  int pct = incomingSensorData.moisture; 

  String params = "device_id=" + String(incomingSensorData.id);
  params += "&raw=" + String(raw_val);
  params += "&pct=" + String(pct);
  params += "&bat=" + String(incomingSensorData.voltage, 1);
  params += "&token=" + String(API_KEY); 

  // 2. Clean URL Construction
  String baseUrl = gcp_url;
  if (baseUrl.endsWith("/")) {
    baseUrl = baseUrl.substring(0, baseUrl.length() - 1);
  }
  String full_url = baseUrl + "/?" + params;
  
  Serial.print("Target URL: ");
  Serial.println(full_url); 

  // 3. Configure HTTP
  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();

  // 4. Set URL
  modem.sendAT("+QHTTPURL=" + String(full_url.length()) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url);
    modem.waitResponse();
  } else {
    Serial.println("Error: URL Setup Failed");
    return;
  }

  // 5. Execute GET
  Serial.println(">> Sending GET Request...");
  modem.sendAT("+QHTTPGET=80");
  
  // 6. DEBUG READ LOOP
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
        
        if (line.indexOf("ERROR") != -1 || line.indexOf("+QHTTPGET: 0,") != -1) {
           Serial.println("!! UPLOAD FAILED (See code above) !!");
           modem.sendAT("+QHTTPREAD=80"); // Read body to see why
           return;
        }
      }
    }
  }
  Serial.println("Timeout: No response from modem.");
}

// --- UPLOAD 2: IMAGE DATA (POST MULTIPART + CHUNKING) ---
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

  // 1. Prepare URL & Host
  String urlStr = gcp_url; 
  // Strip protocol
  int httpsIndex = urlStr.indexOf("https://");
  if (httpsIndex != -1) urlStr = urlStr.substring(httpsIndex + 8);
  // Strip trailing slash
  if (urlStr.endsWith("/")) urlStr = urlStr.substring(0, urlStr.length() - 1);
  
  String host = urlStr;
  String targetPath = "/?token=" + String(API_KEY); // API Key in Query Param

  // 2. Prepare Multipart Boundaries
  String boundary = "------------------------ESP32CamBoundary";
  String bodyHead = "--" + boundary + "\r\n";
  bodyHead += "Content-Disposition: form-data; name=\"image\"; filename=\"cam_capture.jpg\"\r\n";
  bodyHead += "Content-Type: image/jpeg\r\n\r\n";
  String bodyTail = "\r\n--" + boundary + "--\r\n";

  // 3. Calculate Sizes
  size_t bodyLen = bodyHead.length() + fileSize + bodyTail.length();
  
  String httpHeader = "POST " + targetPath + " HTTP/1.1\r\n";
  httpHeader += "Host: " + host + "\r\n";
  httpHeader += "User-Agent: ESP32_EC200U\r\n";
  httpHeader += "Connection: keep-alive\r\n";
  httpHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  httpHeader += "Content-Length: " + String(bodyLen) + "\r\n\r\n"; 

  size_t totalUploadSize = httpHeader.length() + bodyLen;
  Serial.printf("Total Upload Bytes: %d\n", totalUploadSize);

  // 4. Configure Modem
  modem.sendAT("+QHTTPCFG=\"contextid\",1"); modem.waitResponse();
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0"); modem.waitResponse();
  // Enable RAW HEADERS (Critical for Multipart)
  modem.sendAT("+QHTTPCFG=\"requestheader\",1"); modem.waitResponse();

  // 5. Set URL (Base URL needed for DNS)
  String full_url_for_modem = gcp_url; 
  modem.sendAT("+QHTTPURL=" + String(full_url_for_modem.length()) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url_for_modem);
    modem.waitResponse();
  } else {
    Serial.println("Error: URL Set Failed"); file.close(); return;
  }

  // 6. START THE STREAM (The Soda Straw)
  Serial.println(">> Sending Data Stream...");
  // Tell modem we are sending X bytes. Timeout 60s, Input time 60s.
  modem.sendAT("+QHTTPPOST=" + String(totalUploadSize) + ",60,60");
  
  if (modem.waitResponse(20000, "CONNECT") == 1) {
    
    // A. Send Headers
    modem.stream.print(httpHeader);
    delay(50); 
    
    // B. Send Body Head
    modem.stream.print(bodyHead);
    delay(50);

    // C. Send Image File (CHUNKING LOOP)
    uint8_t buf[1024]; // 1KB Buffer
    while (file.available()) {
      int bytesRead = file.read(buf, sizeof(buf));
      modem.stream.write(buf, bytesRead);
      
      // CRITICAL: The "Soda Straw" Delay
      // Prevents UART buffer overflow on the EC200U side
      delay(20); 
    }
    
    // D. Send Body Tail
    modem.stream.print(bodyTail);
    
    Serial.println(">> Data Stream Complete. Waiting for Server...");
    modem.waitResponse(20000); // Wait for OK from modem (not server yet)
  } else {
    Serial.println("Error: Connect Failed"); file.close(); return;
  }
  
  file.close();
  // --- FIX: GIVE THE CLOUD 5 SECONDS TO PROCESS THE IMAGE ---
  Serial.println(">> Waiting for Cloud Processing...");
  delay(5000); 
  // ----------------------------------------------------------

  // 7. Read Server Response
  Serial.println(">> Reading Cloud Response...");

// 7. Read Server Response
  Serial.println(">> Reading Cloud Response...");
  modem.sendAT("+QHTTPREAD=80"); // Read with 80s server-wait timeout
  
  long start = millis();
  bool uploadConfirmed = false;
  
  // Wait up to 10 seconds for the modem to spit out the data
  while (millis() - start < 10000) {
    while (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      line.trim(); // Remove whitespace/newlines to clean it up
      
      if (line.length() > 0) {
        Serial.print("[Cloud]: ");
        Serial.println(line); // Print what the server sent!
        
        // check for success in the JSON body
        if (line.indexOf("success") != -1) {
          uploadConfirmed = true;
        }
        
        // STOP if the modem says it is done
        if (line == "OK" || line == "ERROR") {
           goto finish_read; // Jump out of both loops
        }
      }
    }
  }

finish_read:
  if (uploadConfirmed) {
    Serial.println(">>> UPLOAD CONFIRMED! <<<");
  } else {
    Serial.println(">> Upload finished (Check logs above for status)");
  }
}

// --- 6. SLEEP MANAGER ---
void manageSleep(DateTime now) {
  // Simplified for debugging. 
  // Uncomment below to enable real sleep later.
  /*
  int currentHour = now.hour();
  int currentMin = now.minute();
  
  // Guard Windows logic here...
  // For now, let's keep it running to debug connectivity
  */
}

void setup() {
  Serial.begin(115200);

  // STEP 1: ESP-NOW INIT (First!)
  // This allows us to receive data while Modem connects
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

  // STEP 2: FILESYSTEM
  if (!SPIFFS.begin(true)) Serial.println("SPIFFS Mount Failed!");

  // STEP 3: MODEM SETUP
  if (!rtc.begin()) Serial.println("RTC Failed!");
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
    // If we haven't seen a chunk in 2 seconds, image is done
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

  // 4. Sleep Logic (Currently Disabled/Pass-through)
  if (millis() - lastCheck > 10000) { 
    if (!isReceivingImage && !imageReadyForUpload) {
        manageSleep(rtc.now());
    }
    lastCheck = millis();
  }
}