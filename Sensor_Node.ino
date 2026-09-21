

//Library for ESP-NOW communication
#include <ESP8266WiFi.h>
#include <espnow.h>

//library for DHT11 Temperature and Humidity Sensor
#include "DHT.h"


//define ultrasonic distance sensor pins - trigger and echo
const int TRIG_PIN = D5;
const int ECHO_PIN = D6;

//define DHT11 pin and sensor type
const int DHT_PIN = D7;
const uint8_t DHT_TYPE = DHT11;
//create a object (DHT11)
DHT dht11_sensor(DHT_PIN, DHT_TYPE);

//define Soil moisture sensor pin
const int SOIL_MOISTURE_PIN = A0;


//ultrasonic sensor reading storage variable - water level distance
float waterLevel_distance;

//DHT11 sensor reading storage data type and variable - temperature and humidity
struct dht11Data{
  float temperature;
  float humidity;
};
dht11Data dht11_temp_humidity;

//soil moisture sensor reading storage veriable
float soilMoisture_level;



//define a sensor reading packet data structure that will be used during communication
struct sensorReadingPacket{
  float temperature;
  float humidity;
  int waterLevel;
  int soilMoisture;
};

//create a variable of the sensor reading packet data structure defined
sensorReadingPacket sensorTransmissionPacket;


//store MAC Address for Alert Node (Node 3)
uint8_t alertNode_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; //REDACTED


//ESP-NOW transmission delivery feedback callback handler when data is sent
//This function automatically executes the moment the node attempts to transmit data packets
void OnDataSent(uint8_t *mac_addr, uint8_t sentStatus){
  Serial.print("\r\n Recent Packet sent status: \t");
  Serial.println(sentStatus == 0 ? "Successfully delivered" : "Delivery Failed");
}




void setup() {
  // put your setup code here, to run once:

  //begin serial monitor at 115200
  Serial.begin(115200);

  //wake up the external built-in antenna in the esp8266 and set it into WiFi station mode which is required for ESP-NOW
  WiFi.forceSleepWake();
  delay(10);
  WiFi.mode(WIFI_STA);

  //configure the ultrasonic sensor pins: trigger pin to output mode and echo pin to input
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  //initialise DHT11 sensor
  dht11_sensor.begin();



  //check ESP-NOW connection if failed, add message to serial monitor and deep sleep for 5 seconds
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed");
  }

  //set device role as controller (sender) 
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

  //register the transmission callback function defined
  esp_now_register_send_cb(OnDataSent);

  //register the Alert Node (3) as a reciever 
  //Settings: (MAC Address, Device Role, Wi-Fi Channel 1, No Security Key, Key Length 0)
  esp_now_add_peer(alertNode_MAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0); 

}



void loop() {
  // put your main code here, to run repeatedly:

  //calculate water level by calling the measureWaterLevel function to read ultrasonic distance sensor
  waterLevel_distance = measureWaterLevel();
  
  //get temperature and humidity levels
  dht11_temp_humidity = measureTempHumidity();

  //get soil moisture level
  soilMoisture_level = measureSoilMoisture();


  //now prepare the data packet for transmission
  sensorTransmissionPacket.temperature = dht11_temp_humidity.temperature;
  sensorTransmissionPacket.humidity = dht11_temp_humidity.humidity;
  sensorTransmissionPacket.waterLevel = waterLevel_distance;
  sensorTransmissionPacket.soilMoisture = soilMoisture_Level;

  Serial.println("Brodcasting Data Packet to Alert Node");

  //now transmit the data packet
  esp_now_send(alertNode_MAC, (uint8_t *) &sensorTransmissionPacket, sizeof(sensorTransmissionPacket));

  //wait 3 seconds
  delay(3000);

}



//ultrasonic distance sensor reading function - water level
float measureWaterLevel(){

  // Take ultrasonic distance data
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float calculatedDistance = duration * 0.034 / 2; //multiply by half of speed of sound
  
  Serial.print("Water Distance Measured: ");
  Serial.print(calculatedDistance);
  Serial.print(" cm  |  ");

  return calculatedDistance;
}

//DHT11 sensor reading function - temperature and humidity
dht11Data measureTempHumidity(){
  dht11Data readings;

  //read temperature
  float temperature = dht11_sensor.readTemperature();

  //read humidity
  float humidity = dht11_sensor.readHumidity();

  //check readings are successfull
  if(isnan(temperature) || isnan(humidity)){
    Serial.println("DHT11 readings failed!");
  }
  else{
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print("%");

    Serial.print ("  |  ");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("°C  |  ");

    readings.temperature = temperature;
    readings.humidity = humidity;
  }

  return readings;
}



//soil moisture sensor reading function
float measureSoilMoisture(){
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);

  Serial.print("Soil Moisture: ");
  Serial.println(soilMoistureValue);

  return soilMoistureValue;
}
