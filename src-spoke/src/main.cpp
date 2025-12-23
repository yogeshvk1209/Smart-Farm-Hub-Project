#include <ESP8266WiFi.h>
#include <espnow.h>

// 1. DESTINATION MAC ADDRESS (Your Hub)
uint8_t broadcastAddress[] = {0xC0, 0xCD, 0xD6, 0x85, 0x18, 0x7C};

// 2. DEFINE PINS
const int sensorAnalogPin = A0;

// 3. DEFINE DATA STRUCTURE (Must match Receiver)
typedef struct struct_message {
  int id;          // Node ID
  int moisture;    // Raw Value
} struct_message;

struct_message myData;

// 4. CALLBACK: CONFIRM DELIVERY
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
    Serial.println("Delivery Success");
  } else{
    Serial.println("Delivery Fail");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Init WiFi in Station Mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Set the Role and Register Callback
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);

  // Register the Peer (The Hub)
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  
  Serial.println("ESP8266 Simple Spoke Ready.");
}

void loop() {
  // --- STEP 1: READ ---
  int rawValue = analogRead(sensorAnalogPin);

  // --- STEP 2: PREPARE DATA ---
  myData.id = 1;
  myData.moisture = rawValue;

  // --- STEP 3: SEND ---
  Serial.print("Sending Reading: ");
  Serial.println(rawValue);
  
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  // --- STEP 4: WAIT ---
  delay(60000); // Send every 1 min
}