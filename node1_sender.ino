#include <ESP8266WiFi.h>
#include <espnow.h>

#define TRIG_PIN D5
#define ECHO_PIN D6
#define LED_PIN D2

uint8_t receiverAddress[] = {0x34, 0x94, 0x54, 0x95, 0xAD, 0x61}; //MAC address of node 2

struct __attribute__((packed)) DataPacket {
  float distance;
};
DataPacket sensorData;

void setup() {
  // Let the USB port and power rails stabilize completely upon wake
  delay(200); 
  Serial.begin(115200);
  Serial.println("\n--- Woken Up Successfully ---");
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  // Flash LED to signal active state
  digitalWrite(LED_PIN, HIGH);
  delay(60);
  digitalWrite(LED_PIN, LOW);

  // Take ultrasonic distance data
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  sensorData.distance = duration * 0.034 / 2; //multiply by half of speed of sound
  
  Serial.print("Sensor Measured: ");
  Serial.print(sensorData.distance);
  Serial.println(" cm");

  // Manually force radio power state safely
  WiFi.forceSleepWake();
  delay(10);
  WiFi.mode(WIFI_STA);

  //check ESP-NOW connection if failed, add message to serial monitor and deep sleep for 5 seconds
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed");
    ESP.deepSleep(5e6, WAKE_RF_DISABLED); 
  }

  //set device role as controller (sender) 
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  //register node 2 as reciever 
  //Settings: (MAC Address, Device Role, Wi-Fi Channel 1, No Security Key, Key Length 0)
  esp_now_add_peer(receiverAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  // Broadcast out to Node 2
  esp_now_send(receiverAddress, (uint8_t *) &sensorData, sizeof(sensorData));
  Serial.println("Data package sent out.");

  // Give the radio buffer 100ms to finish pushing the transmission out
  delay(100); 

  Serial.println("Going to sleep for 5 seconds...");
  
  // Sleep safely for 5 seconds with RF disabled at startup to prevent brownouts over USB 
  ESP.deepSleep(5e6, WAKE_RF_DISABLED); 
}

void loop() {
  // Keeps completely empty
}