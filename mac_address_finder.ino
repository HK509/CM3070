
#include <ESP8266WiFi.h>

void setup() {
  // put your setup code here, to run once:
    //start serial monitor
    Serial.begin(9600);
    delay(1000);
    //get MAC address and print it to the serial mointor - only once
    WiFi.mode(WIFI_STA);
    Serial.print("Node 1 (SENSOR NODE) MAC Address is: ");
    Serial.println(WiFi.macAddress());

}

void loop() {
  // put your main code here, to run repeatedly:

}
