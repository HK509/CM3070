
//libraries for ESP-NOW communication
#include <ESP8266WiFi.h>
#include <espnow.h>

//library for LCD screen
#include <LiquidCrystal_I2C.h>

//define LED pins
const int GREEN_LED = D8;
const int YELLOW_LED = D7;
const int RED_LED = D6;

//define buzzer pin
const int BUZZER_PIN = D5;

//create a LCD object
LiquidCrystal_I2C lcdScreen(0x27, 16, 2);

//create variable storing current and previous environment status - (0: good - green, 1: warning - amber, 2: warning - red)
int environmentStatus = 0;
int previousStatus = -1;


//define a sensor reading packet data structure that will be used during communication
struct sensorReadingPacket{
  float temperature;
  float humidity;
  float waterLevel;
  int soilMoisture;
};

//create a variable of the sensor reading packet that will be recieved from the Sensor Node (Node 1)
sensorReadingPacket sensorReadingsRecieved;


//define a mitigation command packet that will be used for communication to the mitigation node
struct mitigationCommandPacket{
  bool activateFloodgate;
  bool activateIrrigation;
  byte communicationCheck;
};

//create a variable of the calculated mitigation action command packet data structure defined that will be sent to the Mitigation Node
mitigationCommandPacket mitigationTransmissionCommand;


//define a sleep duration packet data structure that will be used as acknowledgement to the Sensor Node's sensor reading data packet
struct sleepDurationPacket{
  int sleepDuration;
};

//create a variable of the calculated sleep duration packet data structure defined that will be sent to the Sensor Node as acknowledgement
sleepDurationPacket calculatedSleepDurationPacket;


//store MAC Address for Sensor Node (Node 1)
uint8_t sensorNode_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; //REDACTED

//store MAC Address for Mitigation Node (Node 2)
uint8_t mitigationNode_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; //REDACTED


//ensure that mitigation actions haven't been force enabled on dashboard
bool manualOverrideActive = false;


//ESP-NOW transmission delivery feedback callback handler when data is sent
//This function automatically executes the moment the node attempts to transmit data packets
void onDataSent(uint8_t *mac_addr, uint8_t sentStatus){
  Serial.print("\r\n Recent Packet sent status: \t");
  Serial.println(sentStatus == 0 ? "Successfully delivered" : "Delivery Failed");
}


// Automatically triggers whenever Sensor Node (Node 1) transmits a message packet
void onDataRecv(uint8_t * mac, uint8_t *incomingByte, uint8_t len) {
  
  // Unpack incoming bytes back into structured reading values
  memcpy(&sensorReadingsRecieved, incomingByte, sizeof(sensorReadingsRecieved));
  
  // Print results to serial monitor
  Serial.print("Data Packet Recieved from Sensor Node (Node 1)");
  Serial.print("Temperature: "); Serial.print(sensorReadingsRecieved.temperature); Serial.print(" °C");
  Serial.print(" | ");
  Serial.print("Humidity: "); Serial.print(sensorReadingsRecieved.humidity); Serial.print(" %");
  Serial.print(" | ");
  Serial.print("Water Level Distance: "); Serial.print(sensorReadingsRecieved.waterLevel); Serial.print(" cm");
  Serial.print(" | ");
  Serial.print("Soil Moisture Level: "); Serial.println(sensorReadingsRecieved.soilMoisture);

  //FUZZY LOGIC PLACEHOLDER BELOW ----------------------------
  //now communicate with mitigation node if manual dashboard force override isn't enabled
  if(!manualOverrideActive){
    //automated threshold evaluation (Fuzzy Logic placeholder for now)
    // if the tank water level gets high (e.g., > 10cm) or soil probe detects dry dirt
    if (sensorReadingsRecieved.waterLevel > 10 || sensorReadingsRecieved.soilMoisture < 300) {
      mitigationTransmissionCommand.activateFloodgate = true; //open the floodgate
      mitigationTransmissionCommand.activateIrrigation = false; //deactivate the irrigation system/ waterpump + relay
    } else {
      mitigationTransmissionCommand.activateFloodgate = false; //close the floodgate
      mitigationTransmissionCommand.activateIrrigation = true; //activate the irrigation system/ waterpump + relay
    }
    mitigationTransmissionCommand.communicationCheck = 0xAA; //mark data packet as an automated action frame
  }
  //FUZZY LOGIC PLACEHOLDER ABOVE ------------------------------


  //call the fuzzy logic function to evaluate the environment based on sensor readings recieved
  environmentStatus = fuzzyLogicEnvironmentEvaluation(sensorReadingsRecieved.temperature, 
                                                      sensorReadingsRecieved.humidity, 
                                                      sensorReadingsRecieved.waterLevel, 
                                                      sensorReadingsRecieved.soilMoisture);

  Serial.println("Now communicating with Mitigation Node and sending actions");
  //now transmit the mitigation action control packet to the Mitigation Node
  esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));


  calculatedSleepDurationPacket.sleepDuration = 1000;
  Serial.println("Now communicating with Sensor Node and sending sleep duration as aknowledgement");

  //now transmit the sleep duration packet to the Sensor node
  esp_now_send(sensorNode_MAC, (uint8_t *) &calculatedSleepDurationPacket, sizeof(calculatedSleepDurationPacket));
}



void setup() {
  // put your setup code here, to run once:

  //initialise serial monitor at 115200
  Serial.begin(115200);

  //wake up the external built-in antenna in the esp8266 and set it into WiFi station mode which is required for ESP-NOW
  WiFi.forceSleepWake();
  delay(10);
  WiFi.mode(WIFI_STA);

  //initialise the LCD screen and its backlight
  lcdScreen.init();
  lcdScreen.backlight();

  //display normal message 
  lcdScreen.clear();
  //displayNormalText();

  //initialise LED pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  //initialise buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);

  //check ESP-NOW connection if failed, add message to serial monitor
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed");
  }

  //set device role as both sender and reciever (combo) 
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  //register the transmission callback function defined
  esp_now_register_recv_cb(onDataRecv);

  //register the transmission callback function defined
  esp_now_register_send_cb(onDataSent);

  //register the Sensor Node (Node 1) as both sender and reciever 
  //Settings: (MAC Address, Device Role, Wi-Fi Channel 1, No Security Key, Key Length 0)
  esp_now_add_peer(sensorNode_MAC, ESP_NOW_ROLE_COMBO, 1, NULL, 0); 

  //register the Mitigation Node (Node 2) as a reciever 
  //Settings: (MAC Address, Device Role, Wi-Fi Channel 1, No Security Key, Key Length 0)
  esp_now_add_peer(mitigationNode_MAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0); 

  Serial.println("Receiver online. Awaiting data bursts every 3 seconds...");
}


void loop() {
  // put your main code here, to run repeatedly:

  //check if the environment status has changed and update accordingly
  updateStatus();

  //check if environment status is CRITICAL (we want to continously loop over the siren until the state stops)
  if(environmentStatus == 2){
    playSiren();
  }

  //randomly go through the three states - 0: normal (green), 1: WARNING (yellow), and 2: CRITICAL (red)
  environmentStatus = random(0, 3);
  Serial.print("Environment Status: ");
  Serial.print(environmentStatus);
  Serial.print(" | ");
  Serial.print("Previous Status: ");
  Serial.println(previousStatus);
  delay(3000);

}


//function that checks if environment status has changed, if it has, act accordingly and update previousStatus
void updateStatus(){

  //change LEDs illumination based on new current envirconment condition status
  operateLeds();

  //status changed to normal
  if(environmentStatus == 0 && previousStatus !=0){
    lcdScreen.clear();
    displayNormalText();

    //clear the buzzer in case siren was enabled for CRITICAL state
    noTone(BUZZER_PIN);

    previousStatus = 0;
  } 

  //status changed to WARNING
  if(environmentStatus == 1 && previousStatus !=1){
    lcdScreen.clear();
    displayWarningText();

    //clear the buzzer in case the siren was enbaled for CRITICAL state
    noTone(BUZZER_PIN);
    //play the warning beep - we only want it once when the warning state is identified
    playWarningAlarm();

    previousStatus = 1;
  }

  //status changed to CRITICAL
  if(environmentStatus == 2 && previousStatus !=2){
    lcdScreen.clear();
    displayCriticalText();
    previousStatus = 2;
  }
}


//function that displays corresponding text when the environment is in a normal state
void displayNormalText() {
  lcdScreen.setCursor(0, 0);       
  lcdScreen.print("Alert Node");                  
  lcdScreen.setCursor(0, 1);       
  lcdScreen.print("Normal");
}

//function that displays corresponding text when the environment is in a WARNING state
void displayWarningText() {
  lcdScreen.setCursor(0, 0);       
  lcdScreen.print("Alert Node");                  
  lcdScreen.setCursor(0, 1);       
  lcdScreen.print("WARNING");
}

//function that displays corresponding text when the environment is in a CRITICAL state
void displayCriticalText() {
  lcdScreen.setCursor(0, 0);       
  lcdScreen.print("Alert Node");                  
  lcdScreen.setCursor(0, 1);       
  lcdScreen.print("CRITICAL");
}


//function that controls the LEDs based on current environmental status 
void operateLeds(){
  if(environmentStatus == 0){
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);
  }

  if(environmentStatus == 1){
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  }

  if(environmentStatus == 2){
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }
}

//function that sounds a small beep alarm from buzzer when the environment status is set to WARNING
void playWarningAlarm(){
  tone(BUZZER_PIN, 450, 500);
  delay(500);
  tone(BUZZER_PIN, 450, 500);
}

//function that plays a siren noise buzzer continuously when the environment status is set to CRITICAL
void playSiren(){

  //two-tone siren
  //tone(BUZZER_PIN, 500, 750);
  //delay(500);
  //tone(BUZZER_PIN, 600, 750);
  //delay(500);

  //smooth constant shifting wail siren
  //gradually increase frequency
  for(int frequency = 500; frequency <= 800; frequency += 5){
    tone(BUZZER_PIN, frequency);
    delay(10);
  }
  //gradually decrease frequency
  for(int frequency = 800; frequency >= 500; frequency -= 5){
    tone(BUZZER_PIN, frequency);
    delay(10);
  }
}


//fuzzy logic evaluation function that returns the environment status: 0 - Normal, 1 - WARNING, 2 - CRITICAL
int fuzzyLogicEnvironmentEvaluation(float temperature, float humidity, float waterLevel, int soilMoisture){

  //calculate natural disaster threat indexes (between 0 and 1) using sensor readings - Fuzzification
  //0.0 = safe, no risk, 1.0 = maximum threat/ saturation

  //use right handed trapizoid function to calculate the temperature threat for wildfires, droughts and floods (because temperature has no upper limit except sensor limit)
  //risk begins at 23.5°C and saturates (always 1.0 onwards) from 30.0°C (risk increases as temperature increases)
  float temperatureRisk = rightHandedTrapizoid(temperature, 23.5, 30.0);

  //use right handed trapiziod function to calculate the water level threat for floods
  //risk begins at 1.5cm and saturates (always 1.0 onwards) from 3.0cm (risk increases as water level increases)
  float waterLevelFloodRisk = rightHandedTrapizoid(waterLevel, 1.5, 3.0);

  //use left handed trapiziod function to calculate the humidity risk for floods, wildfires and drought
  //risk begins at 65.0% humidity and saturates (always 1.0 at this point or below) at 40% humidity
  float humidityRisk = leftHandedTrapizoid(humidity, 65.0, 45.0);

  //use left handed trapiziod function to calculate the soil moisture level risk for wildfires and drought
  //risk begins at a soil moisture level of 700.0 and saturates (always 1.0 at this point or below) at 300.0 soil moisture level
  float soilMoistureRisk = leftHandedTrapizoid(float(soilMoisture), 700.0, 300.0);


  //now calculate risk index for each disaster using Mumdani-Style Fuzzy Inference
  //min mapping is equal to AND, max mapping is equal to OR
  
  //calculate wildfire risk - high temperature, low humidity and low soil moisture (dry soil)
  //using minimum mapping - e.g. the temperature may be high and humidity may be low, but if the soil is not dry then the risk of wildfire is not too great
  //Rule: If temperature is high, and humidity is low and soil moisture is low (dry), then the threat of wildfire is critical
  float wildfireRisk = min(temperatureRisk, min(humidityRisk, soilMoistureRisk));

  //calculate drought risk - high temperature and normal soil moisture (soil is getting drier)
  //using minimum mapping - e.g. the temperature may be high, but if the soil is not dry then the risk of drought is not too great until it starts getting drier
  //Rule: If temperature is high and the soil moisture is normal to low, then the threat of drought is warning
  float droughtRisk = min(temperatureRisk, soilMoistureRisk);

  //calculate flood risk - high temperature, high humidity and high water levels
  //using minimum and maximum mapping - the risk of flood is increased when water level is rising, but if water levels are low due to heat then flood risk is not great
  //Rule: If temperature is high, and humidity is low, or the water level is rising, then the threat of flood is critical
  float floodRisk = max(waterLevelFloodRisk, min(humidityRisk, temperatureRisk));


  return 0; //fallback for now
}


//Fuzzy Logic helper function - Right Handed Trapiziod function - risk increases as factor increases
float rightHandedTrapizoid(float value, float startPoint, float saturationPoint){
  //completely safe conditions
  if(value <= startPoint){
    return 0.0;
  }

  //maximum threat saturation conditions
  if(value >= saturationPoint){
    return 1.0;
  }

  //fallback - not the two edge cases (completely safe/ maximum threat) so calculate threat index
  return (value - startPoint) / (saturationPoint - startPoint);
}


//Fuzzy Logic helper function - Left Handed Trapiziod function - risk increases as factor decreases
float leftHandedTrapizoid(float value, float endPoint, float saturationPoint){
  //maximum threat saturation conditions - factor is too low
  if(value <= saturationPoint){
    return 1.0;
  }

  //completely safe conditions - factor is high enough
  if(value >= endPoint){
    return 0.0;
  }

  //fallback - not the two edge cases (completely safe/ maximum threat) so calculate threat index
  return (endPoint - value) / (endPoint - saturationPoint);
}


