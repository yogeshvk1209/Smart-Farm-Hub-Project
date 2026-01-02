#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_camera.h>
#include <ESPNowCam.h>

// 1. CONFIGURATION
// REPLACE WITH REAL HUB MAC
uint8_t broadcastAddress[] = {0xC0, 0xCD, 0xD6, 0x85, 0x18, 0x7C}; 

// Global Flag for ACK
volatile bool ackReceived = false;

// CAMERA PINS (AI Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// --- 1. NATIVE CALLBACK (THE TRUTH TELLER) ---
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    ackReceived = true;
  }
}

void deepSleep(int minutes) {
  Serial.printf(">> Sleeping for %d minutes...\n", minutes);
  Serial.flush();
  uint64_t sleepTime = (uint64_t)minutes * 60 * 1000000;
  esp_deep_sleep(sleepTime);
}

// --- 2. CAMERA ROUTINE (Only runs if Hub is found) ---
void runCameraSequence() {
  Serial.println(">> Hub Confirmed! Starting Camera...");
  
  // A. Init Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 12; 
  config.frame_size = FRAMESIZE_SVGA; 
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera Init Failed");
    return;
  }

  // B. Init Library (This might overwrite our callback, which is fine now)
  ESPNowCam cam;
  if (!cam.init()) {
    Serial.println("ESPNowCam Lib Init Failed");
    return;
  }
  cam.setTarget(broadcastAddress);

  // C. Capture & Send
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Capture failed");
    return;
  }

  Serial.printf("Sending Image (%d bytes)...\n", fb->len);
  
  // NOTE: sendData also returns true blindly sometimes, 
  // but we already know the Hub is awake, so it's safer now.
  if (cam.sendData(fb->buf, fb->len)) {
    Serial.println("Transfer Finished.");
  } else {
    Serial.println("Transfer Failed.");
  }
  
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Spoke 2: TRUE Hunter Mode ---");

  // 1. Manual WiFi & ESP-NOW Setup
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    deepSleep(2);
  }

  // 2. REGISTER THE TRUTH CALLBACK
  esp_now_register_send_cb(OnDataSent);

  // 3. Register Peer Manually (For the Ping)
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1; // MUST MATCH HUB CHANNEL
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    deepSleep(2);
  }

  // 4. THE PING
  uint8_t data = 0x01;
  ackReceived = false; // Reset flag
  esp_err_t result = esp_now_send(broadcastAddress, &data, 1);

  if (result == ESP_OK) {
    Serial.print("Ping Sent. Waiting for ACK...");
  } else {
    Serial.println("Ping Send Error (Internal).");
    deepSleep(2);
  }

  // 5. WAIT FOR ACK (Max 200ms)
  unsigned long start = millis();
  while (millis() - start < 200) {
    if (ackReceived) break;
    delay(10);
  }

  // 6. DECISION TIME
  if (ackReceived) {
    Serial.println("\n>>> ACK CONFIRMED! Hub is Awake.");
    // Now we spend the energy
    runCameraSequence();
    deepSleep(30);
  } else {
    Serial.println("\n>>> NO ACK. Hub is sleeping/offline.");
    deepSleep(2);
  }
}

void loop() {
}