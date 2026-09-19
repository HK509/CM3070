
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


void setup() {
  // put your setup code here, to run once:

  //begin serial monitor
  Serial.begin(9600);

  //configure the ultrasonic sensor pins: trigger pin to output mode and echo pin to input
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  //initialise DHT11 sensor
  dht11_sensor.begin();

}



void loop() {
  // put your main code here, to run repeatedly:

  //calculate water level by calling the measureWaterLevel function to read ultrasonic distance sensor
  waterLevel_distance = measureWaterLevel();
  
  //get temperature and humidity levels
  dht11_temp_humidity = measureTempHumidity();

  //get soil moisture level
  soilMoisture_level = measureSoilMoisture();

  delay(750);

  

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
