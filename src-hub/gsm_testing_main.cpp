#include <Arduino.h>

// Pins
#define MODEM_PWRKEY 18 
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17

HardwareSerial modemSerial(2);

void setup() {
  Serial.begin(115200);
  
  // Turn on Modem
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH); 
  
  modemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(100);
  
  // Check if alive, if not pulse power
  modemSerial.println("AT");
  delay(100);
  if (!modemSerial.available()) {
    Serial.println("Powering up modem...");
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(2000);
    digitalWrite(MODEM_PWRKEY, HIGH);
  }
  Serial.println("Ready! Type AT commands here.");
}

void loop() {
  if (modemSerial.available()) Serial.write(modemSerial.read());
  if (Serial.available()) modemSerial.write(Serial.read());
}
