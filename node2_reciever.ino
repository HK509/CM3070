#include <ESP8266WiFi.h>
#include <espnow.h>

#define LED_PIN D7

struct __attribute__((packed)) DataPacket {
  float distance;
};
DataPacket incomingData;

// Automatically triggers whenever Node 1 transmits a message packet
void onDataRecv(uint8_t * mac, uint8_t *incomingByte, uint8_t len) {
  // Turn LED on immediately upon message detection
  digitalWrite(LED_PIN, HIGH);
  
  // Unpack incoming bytes back into structured reading values
  memcpy(&incomingData, incomingByte, sizeof(incomingData));
  
  // Print results to serial monitor
  Serial.print("Data Received Distance: ");
  Serial.print(incomingData.distance);
  Serial.println(" cm");
  
  // Turn LED off after a quick visual pulse
  delay(100);
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Put Wi-Fi antenna into listening station mode
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Initialisation Failed.");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onDataRecv);
  
  Serial.println("Receiver online. Awaiting data bursts every 5 seconds...");
}

void loop() {
  // Free execution loop. The background radio automatically handles the onDataRecv callback.
}