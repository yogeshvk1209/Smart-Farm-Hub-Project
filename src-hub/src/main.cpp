#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// --- 1. MODEM CONFIGURATION ---
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024 
#include <TinyGsmClient.h>

// Pins
#define MODEM_PWRKEY 18 
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17

// --- CONFIGURATION ---
// We use 'gcp_url' everywhere now.
String gcp_url = "https://ingest-farm-data-362443017574.asia-south1.run.app"; 

// Objects
HardwareSerial modemSerial(2);
TinyGsm modem(modemSerial);

// --- 2. DATA STRUCTURES ---
typedef struct struct_message {
  int id;
  int moisture;
} struct_message;

struct_message incomingData;
volatile bool newDataReceived = false;

// --- 3. ESP-NOW CALLBACK ---
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingBytes, int len) {
  memcpy(&incomingData, incomingBytes, sizeof(incomingData));
  newDataReceived = true; 
}

// --- 4. MANUAL GET UPLOAD (The Fix) ---
void sendManualGET(String params) {
  Serial.println("\n>>> STARTING GET UPLOAD <<<");

  // 1. Configure Context
  modem.sendAT("+QHTTPCFG=\"contextid\",1");
  modem.waitResponse();

  // 2. SSL Config
  modem.sendAT("+QHTTPCFG=\"sslctxid\",1"); 
  modem.waitResponse();
  modem.sendAT("+QSSLCFG=\"seclevel\",1,0");
  modem.waitResponse();

  // 3. Construct URL
  // Uses 'gcp_url' correctly now
  String full_url = gcp_url + "/?" + params;
  int urlLen = full_url.length();
  
  Serial.print("Target: "); Serial.println(full_url);

  // 4. Set URL
  modem.sendAT("+QHTTPURL=" + String(urlLen) + ",80");
  if (modem.waitResponse(10000, "CONNECT") == 1) {
    modem.stream.print(full_url);
    modem.waitResponse();
  } else {
    Serial.println("Error: URL Setup Failed");
    return;
  }

  // 5. Send GET Request
  Serial.println("Sending Request...");
  modem.sendAT("+QHTTPGET=60"); 
  
  // 6. Read Response
  long start = millis();
  while (millis() - start < 15000) {
    while (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      Serial.print("RX: "); Serial.println(line); 
      // Check for HTTP 200
      if (line.indexOf("200") != -1 && line.indexOf("QHTTP") == -1) {
         Serial.println(">>> SUCCESS: 200 OK Received <<<");
      }
    }
    delay(100);
  }
  Serial.println(">>> CYCLE DONE <<<\n");
}

// --- 5. JIO CONNECTION SEQUENCE ---
bool connectToJio() {
  Serial.println("\n--- JIO SETUP: START ---");
  
  modem.sendAT("+QIDEACT=1");
  modem.waitResponse();

  Serial.print("Setting IPv4v6... ");
  modem.sendAT("+QICSGP=1,3,\"jionet\",\"\",\"\",0");
  if (modem.waitResponse() == 1) Serial.println("OK");
  else return false;

  Serial.print("Activating... ");
  modem.sendAT("+QIACT=1");
  if (modem.waitResponse(10000) == 1) Serial.println("OK");
  else return false;

  Serial.println("--- JIO SETUP: COMPLETE ---\n");
  return true;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); 
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(1000);

  Serial.println("System Initializing...");
  modem.restart(); 
  
  if (connectToJio()) {
    Serial.println("Hub Online.");
  } else {
    Serial.println("Hub Offline (Modem Error).");
  }

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  
  Serial.println(">>> WAITING FOR SPOKE DATA <<<");
}

void loop() {
  if (newDataReceived) {
    int pct = map(incomingData.moisture, 1024, 420, 0, 100);
    pct = constrain(pct, 0, 100);
    
    // Create Params
    String params = "device_id=spoke_" + String(incomingData.id);
    params += "&raw=" + String(incomingData.moisture);
    params += "&pct=" + String(pct);
    params += "&bat=0";

    // Call the correct function
    sendManualGET(params);
    
    newDataReceived = false; 
  }
}