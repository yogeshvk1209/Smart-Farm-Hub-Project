#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_camera.h>

// 1. CONFIGURATION
// REPLACE WITH YOUR HUB MAC ADDRESS
uint8_t broadcastAddress[] = {0xC0, 0xCD, 0xD6, 0x85, 0x18, 0x7C}; 

#define MAX_PACKET_SIZE 240 
// RTC_DATA_ATTR int lastKnownHour = 0;
RTC_DATA_ATTR int missCount = 0;
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

// --- CALLBACKS ---
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) ackReceived = true;
}

void deepSleep(int minutes) {
  Serial.printf(">> Sleeping for %d minutes...\n", minutes);
  Serial.flush();
  esp_deep_sleep((uint64_t)minutes * 60 * 1000000);
}

// --- THE FIX: MANUAL CHUNKING ---
void sendImageChunked(const uint8_t *data, size_t len) {
  uint16_t totalPackets = (len + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
  Serial.printf("Sending %d bytes in %d packets\n", len, totalPackets);

  for (size_t i = 0; i < len; i += MAX_PACKET_SIZE) {
    size_t chunkSize = (i + MAX_PACKET_SIZE > len) ? (len - i) : MAX_PACKET_SIZE;
    
    // Retry Logic
    for (int retries=0; retries<3; retries++) {
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)(data + i), chunkSize);
        if (result == ESP_OK) break; 
        delay(10);
    }
    
    // --- CRITICAL DELAY ---
    // This gives the Hub time to write to SPIFFS before the next chunk hits.
    delay(40); 
    // ----------------------
    
    if (i % 2400 == 0) Serial.print("."); // Progress dot every 10 packets
  }
  Serial.println("\nImage Sent.");
}

void runCameraSequence() {
  Serial.println(">> Hub Confirmed! Starting Camera...");
  
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
  
  // --- UPGRADE SETTINGS ---
  // Quality: 10 (High). Don't go below 10 or it might crash.
  config.jpeg_quality = 10; 
  
  // Resolution: HD (1280x720). 
  // Alternatives: FRAMESIZE_XGA (1024x768), FRAMESIZE_SXGA (1280x1024)
  config.frame_size = FRAMESIZE_SVGA; 
  
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera Init Failed");
    return;
  }

  // Warmup (Important for Auto-Exposure to adjust to light)
  delay(2000); 

  // Capture
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Capture failed");
    return;
  }

  // Send
  Serial.printf("Captured %d bytes (SVGA Mode). Sending...\n", fb->len);
  sendImageChunked(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) deepSleep(2);
  esp_now_register_send_cb(OnDataSent);

  // Register Hub
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1; 
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) deepSleep(2);

  // Ping Hub
  uint8_t data = 0x01;
  ackReceived = false;
  esp_now_send(broadcastAddress, &data, 1);
  
  unsigned long start = millis();
  while (millis() - start < 500) { // Increased wait for Hub ACK
    if (ackReceived) break;
    delay(10);
  }

// ... existing Ping Hub logic ...

if (ackReceived) {
    missCount = 0; // Reset misses on success
    Serial.println("\n>>> HUB ONLINE! Capturing...");
    runCameraSequence(); //
    delay(1000); 
    deepSleep(15); // SUCCESS: Wake up in 15 mins for the next photo
  } else {
    missCount++; // Increment consecutive misses
    Serial.printf("\n>>> NO ACK. Miss count: %d\n", missCount);
    
    // Logic: If we've missed 2 pings in a row (~30 mins of silence), 
    // it's likely after 19:00 (Night Mode).
    if (missCount >= 2) {
        Serial.println(">> Hub consistently dark. Entering Night Mode Sleep...");
        missCount = 0; // Reset for a clean start in the morning
        deepSleep(600); // Sleep 10 hours to bridge the night gap until 07:00 AM
    } else {
        Serial.println(">> Hub missed. Retrying in 15 mins to confirm Night Mode.");
        deepSleep(15); // First miss: retry quickly to confirm if it's just a signal glitch
    }
  }
}  

void loop() {}