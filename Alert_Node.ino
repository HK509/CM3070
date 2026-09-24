
//libraries for ESP-NOW communication
#include <ESP8266WiFi.h>
#include <espnow.h>

//additional libraries for local web dashboard
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>


//library for LCD screen
#include <LiquidCrystal_I2C.h>


//WiFi network connectivity variables
const char* ssid = "//REDACTED";
const char* password = "//REDACTED";

//Static IP Configuration Variables
IPAddress local_IP(192, REDACTED, REDACTED, REDACTED);
IPAddress gateway(192, REDACTED, REDACTED, REDACTED);
IPAddress subnet(255, REDACTED, REDACTED, REDACTED);

//set the PORT for web server
ESP8266WebServer server(80);


//define LED pins
const int GREEN_LED = D8;
const int YELLOW_LED = D7;
const int RED_LED = D6;

//define buzzer pin
const int BUZZER_PIN = D5;

//create a LCD object
LiquidCrystal_I2C lcdScreen(0x27, 16, 2);

//create variables storing current and previous environment status - (0: good - green, 1: warning - amber, 2: warning - red)
int environmentStatus = 0;
int previousStatus = -1;

//create a variable to see if environment status has been overidden in Node RED dashboard - (-1: auto, 0: force normal, 1: force warning, 2: force critical)
int environmentStateOverride = -1;

//create variables to see if a disaster has been evaluated and the previous - (0: normal/ none, 1: wildfire, 2: flood, 3: drought) 
int disasterType = 0;
int previousDisasterType = -1;

//initialise fuzzy logic sensor thresholds
float temperatureWarning = 23.5; 
float temperatureCritical = 30.0; 
float humidityWarning = 65.0; 
float humidityCritical = 45.0; 
float waterLevelWarning = 5.0;
float waterLevelCritical = 3.0;
float soilMoistureWarning = 700;
float soilMoistureCritical = 300;


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

//set default sleep duration values (seconds) and store as variables
int normalSleepDuration = 300;
int warningSleepDuration = 60;
int criticalSleepDuration = 30;

//define sensor reading history object that will be stored in the JSON structure and called by the local web dashboard
struct dataHistoryObj{
  String timestamp;
  float temperature;
  float humidity;
  float waterLevel;
  int soilMoisture;
  int environmentStatus;
  int disasterType;
};

//declare and fix the memory space required by the JSON data for the history log used by local web dashboard
//maximum 15 entries as not to block memory, which will be overritten if filled by new incoming sensor reafings
const int max_entries = 15;
dataHistoryObj historyLog[max_entries];
int currentFilledHistoryIndex = 0;
int totalHistoryEntries = 0;


//store MAC Address for Sensor Node (Node 1)
uint8_t sensorNode_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; //REDACTED


//store MAC Address for Mitigation Node (Node 2)
uint8_t mitigationNode_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; //REDACTED



//ensure that mitigation actions haven't been force enabled on dashboard
bool manualOverrideMitigationActive = false;
//forced floodgate - 'F': Activate, 'K': Deactivate
char forcedFloodgateAction = 'K';
//forced irrigation - 'I': Activate, 'O': Deactivate
char forcedIrrigationAction = 'O';


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


  //go through fuzzy logic
  //call the fuzzy logic function to evaluate the environment based on sensor readings recieved
  environmentStatus = fuzzyLogicEnvironmentEvaluation(sensorReadingsRecieved.temperature, 
                                                      sensorReadingsRecieved.humidity, 
                                                      sensorReadingsRecieved.waterLevel, 
                                                      sensorReadingsRecieved.soilMoisture);
                                                      

  //now communicate with mitigation node if manual dashboard force override isn't enabled
  if(!manualOverrideMitigationActive){
    //now transmit the mitigation action control packet to the Mitigation Node
    Serial.println("Now communicating with Mitigation Node and sending actions");
    esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
  }


  //now transmit the sleep duration packet to the Sensor node
  Serial.println("Now communicating with Sensor Node and sending sleep duration as aknowledgement");
  esp_now_send(sensorNode_MAC, (uint8_t *) &calculatedSleepDurationPacket, sizeof(calculatedSleepDurationPacket));
}



void setup() {
  // put your setup code here, to run once:

  //initialise serial monitor at 115200
  Serial.begin(115200);

  //wake up the external built-in antenna in the esp8266 and set it into WiFi station mode which is required for ESP-NOW
  WiFi.forceSleepWake();
  delay(10);
  WiFi.mode(WIFI_AP_STA);
  wifi_set_channel(1);

  //now apply static ip before WiFi.begin
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Static IP Failed to configure");
  }

  //connect to the WiFi nwtwork
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  //for now keep the chip in the microcontroller waiting, otherwise the fallback will be captive portal
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.println("waiting to connect to WiFi");
  } 

  //Wi-Fi connected, now print it to serial monitor
  Serial.println("Wi-Fi connected successfully!");
  Serial.print("Local Dashboard URL: http://");
  Serial.println(WiFi.localIP());
  
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

  Serial.println("Receiver online. Awaiting data bursts...");

  //define the webserver pages
  server.on("/", getIndex);
  server.on("/history", getHistory);
  //start the webserver
  server.begin();
  Serial.println("Server listening");
}


void loop() {
  // put your main code here, to run repeatedly:

  //listen to webserver if anyone is trying to view it, handle incoming client requests
  server.handleClient();
  yield();

  //listen to serial monitor to see if any fuzzy logic thresholds have been changed, the environment status has been overriden, or deep sleep durations have been changed, or forced mitigation actions
  if(Serial.available() > 0){
    //get the leading character identifier to map the variable threshold type
    char identifier = Serial.read();

    //change fuzzy logic variable thresholds
    //now check which variable to change
    if(identifier == 'A'){
      temperatureWarning = Serial.parseFloat();
      Serial.print("temperatureWarning updated to: ");
      Serial.println(temperatureWarning);
    }
    else if(identifier == 'B'){
      temperatureCritical = Serial.parseFloat();
      Serial.print("temperatureCritical updated to: ");
      Serial.println(temperatureCritical);
    }
    else if(identifier == 'C'){
      humidityWarning = Serial.parseFloat();
      Serial.print("humidityWarning updated to: ");
      Serial.println(humidityWarning);
    }
    else if(identifier == 'D'){
      humidityCritical = Serial.parseFloat();
      Serial.print("humidityCritical updated to: ");
      Serial.println(humidityCritical);
    }
    else if(identifier == 'E'){
      waterLevelWarning = Serial.parseFloat();
      Serial.print("waterLevelWarning updated to: ");
      Serial.println(waterLevelWarning);
    }
    else if(identifier == 'G'){
      waterLevelCritical = Serial.parseFloat();
      Serial.print("waterLevelCritical updated to: ");
      Serial.println(waterLevelCritical);
    }
    else if(identifier == 'H'){
      soilMoistureWarning = Serial.parseFloat();
      Serial.print("soilMoistureWarning updated to: ");
      Serial.println(soilMoistureWarning);
    }
    else if(identifier == 'J'){
      soilMoistureCritical = Serial.parseFloat();
      Serial.print("soilMoistureCritical updated to: ");
      Serial.println(soilMoistureCritical);
    }

    //change environmentStatus/ force the environment state and disaster type - immediate override
    else if(identifier == 'S'){
      environmentStateOverride = Serial.parseInt();
      Serial.print("environmentStateOverride updated to: ");
      Serial.println(environmentStateOverride);

      //ensure override takes place:
      //normal status
      if(environmentStateOverride == 0){
        environmentStatus = 0;
        disasterType = 0;
        //deactivate both floodgate and irrigation
        //activate irrigation
          mitigationTransmissionCommand.activateFloodgate = false;
          mitigationTransmissionCommand.activateIrrigation = false;
          mitigationTransmissionCommand.communicationCheck = 0xCC;
          esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
      }
      //warning status
      else if(environmentStateOverride == 1 || environmentStateOverride == 2 || environmentStateOverride == 3){
        environmentStatus = 1;
        //wildfire
        if(environmentStateOverride == 1){
          disasterType = 1;
          //activate irrigation
          mitigationTransmissionCommand.activateFloodgate = false;
          mitigationTransmissionCommand.activateIrrigation = true;
          mitigationTransmissionCommand.communicationCheck = 0xCC;
          esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
        }
        //flood
        else if(environmentStateOverride == 2){
          disasterType = 2;
          //activate floodgate
          //activate irrigation
          mitigationTransmissionCommand.activateFloodgate = true;
          mitigationTransmissionCommand.activateIrrigation = false;
          mitigationTransmissionCommand.communicationCheck = 0xCC;
          esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
        }
        //drought
        else{
          disasterType = 3;
          //activate irrigation
          mitigationTransmissionCommand.activateFloodgate = false;
          mitigationTransmissionCommand.activateIrrigation = true;
          mitigationTransmissionCommand.communicationCheck = 0xCC;
          esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
        }
      }
      //critical status
      else if(environmentStateOverride == 4 || environmentStateOverride == 5){
        environmentStatus = 2;
        //wildfire
        if(environmentStateOverride == 4){
          disasterType = 1;
          //deactivate both floodgate and irrigation
          mitigationTransmissionCommand.activateFloodgate = false;
          mitigationTransmissionCommand.activateIrrigation = false;
          mitigationTransmissionCommand.communicationCheck = 0xCC;
          esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
        }
        //flood
        else{
          disasterType = 2;
          //activate floodgate
          mitigationTransmissionCommand.activateFloodgate = true;
          mitigationTransmissionCommand.activateIrrigation = false;
          mitigationTransmissionCommand.communicationCheck = 0xCC;
          esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
        }
      }
    }

    //change deep sleep durations
    else if(identifier == 'X'){
      normalSleepDuration = Serial.parseInt();
      Serial.print("normalSleepDuration set to: ");
      Serial.println(normalSleepDuration);
    }
    else if(identifier == 'Y'){
      warningSleepDuration = Serial.parseInt();
      Serial.print("warningSleepDuration set to: ");
      Serial.println(warningSleepDuration);
    }
    else if(identifier == 'Z'){
      criticalSleepDuration = Serial.parseInt();
      Serial.print("criticalSleepDuration set to: ");
      Serial.println(criticalSleepDuration);
    }

    //forced mitigation controls
    //activate floodgate
    else if(identifier == 'F'){
      manualOverrideMitigationActive = true;
      forcedFloodgateAction = 'F';
      forcedIrrigationAction = 'O';
      mitigationTransmissionCommand.activateFloodgate = true;
      mitigationTransmissionCommand.activateIrrigation = false;
      mitigationTransmissionCommand.communicationCheck = 0xFF;
      esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
    }
    //deactivate floodgate
    else if(identifier == 'K'){
      manualOverrideMitigationActive = true;
      forcedFloodgateAction = 'K';
      forcedIrrigationAction = 'O';
      mitigationTransmissionCommand.activateFloodgate = false;
      mitigationTransmissionCommand.activateIrrigation = false;
      mitigationTransmissionCommand.communicationCheck = 0xFF;
      esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
    }
    //activate irrigation
    else if(identifier == 'I'){
      manualOverrideMitigationActive = true;
      forcedFloodgateAction = 'K';
      forcedIrrigationAction = 'I';
      mitigationTransmissionCommand.activateFloodgate = false;
      mitigationTransmissionCommand.activateIrrigation = true;
      mitigationTransmissionCommand.communicationCheck = 0xFF;
      esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
    }
    //deactivate irrigation
    else if(identifier == 'O'){
      manualOverrideMitigationActive = true;
      forcedFloodgateAction = 'K';
      forcedIrrigationAction = 'O';
      mitigationTransmissionCommand.activateFloodgate = false;
      mitigationTransmissionCommand.activateIrrigation = false;
      mitigationTransmissionCommand.communicationCheck = 0xFF;
      esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
    }
    //release controls
    else if(identifier == 'R'){
      manualOverrideMitigationActive = false;
      forcedFloodgateAction = 'K';
      forcedIrrigationAction = 'O';
      mitigationTransmissionCommand.activateFloodgate = false;
      mitigationTransmissionCommand.activateIrrigation = false;
      mitigationTransmissionCommand.communicationCheck = 0xFF;
      esp_now_send(mitigationNode_MAC, (uint8_t *) &mitigationTransmissionCommand, sizeof(mitigationTransmissionCommand));
    }

    else{

    }
  }

  //check if the environment status has changed and update accordingly
  updateStatus();

  //check if environment status is CRITICAL (we want to continously loop over the siren until the state stops)
  if(environmentStatus == 2){
    playSiren();
  }

  //randomly go through the three states - 0: normal (green), 1: WARNING (yellow), and 2: CRITICAL (red)
  //environmentStatus = random(0, 3);
  //Serial.print("Environment Status: ");
  //Serial.print(environmentStatus);
  //Serial.print(" | ");
  //Serial.print("Previous Status: ");
  //Serial.println(previousStatus);
  //delay(3000);

}


//function that checks if environment status has changed, if it has, act accordingly and update previousStatus
void updateStatus(){

  //change LEDs illumination based on new current envirconment condition status
  operateLeds();

  //status changed to normal
  if(environmentStatus == 0 && previousStatus !=0 || ((disasterType != previousDisasterType) && environmentStatus==0)){
    lcdScreen.clear();
    displayNormalText();

    //clear the buzzer in case siren was enabled for CRITICAL state
    noTone(BUZZER_PIN);

    previousStatus = 0;
    previousDisasterType = disasterType;
    Serial.print("Current environmentStatus: ");
    Serial.print(environmentStatus);
    Serial.print(" | ");
    Serial.print("disasterType: ");
    Serial.println(disasterType);

  } 

  //status changed to WARNING
  if(environmentStatus == 1 && previousStatus !=1 || ((disasterType != previousDisasterType) && environmentStatus==1)){
    lcdScreen.clear();
    displayWarningText();

    //clear the buzzer in case the siren was enbaled for CRITICAL state
    noTone(BUZZER_PIN);
    //play the warning beep - we only want it once when the warning state is identified
    playWarningAlarm();

    previousStatus = 1;
    previousDisasterType = disasterType;
    Serial.print("Current environmentStatus: ");
    Serial.print(environmentStatus);
    Serial.print(" | ");
    Serial.print("disasterType: ");
    Serial.println(disasterType);
  }

  //status changed to CRITICAL
  if(environmentStatus == 2 && previousStatus !=2 || ((disasterType != previousDisasterType) && environmentStatus==2)){
    lcdScreen.clear();
    displayCriticalText();
    previousStatus = 2;
    previousDisasterType = disasterType;
    Serial.print("Current environmentStatus: ");
    Serial.print(environmentStatus);
    Serial.print(" | ");
    Serial.print("disasterType: ");
    Serial.println(disasterType);
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
  if(disasterType == 1){     
    lcdScreen.print("WARNING - W");
  }
  else if(disasterType == 2){     
    lcdScreen.print("WARNING - F");
  }
  else if (disasterType == 3){     
    lcdScreen.print("WARNING - D");
  }
}

//function that displays corresponding text when the environment is in a CRITICAL state
void displayCriticalText() {
  lcdScreen.setCursor(0, 0);       
  lcdScreen.print("Alert Node");                  
  lcdScreen.setCursor(0, 1);
  if(disasterType == 1){      
    lcdScreen.print("CRITICAL - W");
  }
  else if(disasterType == 2){      
    lcdScreen.print("CRITICAL - F");
  }
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
  float temperatureRisk = rightHandedTrapizoid(temperature, temperatureWarning, temperatureCritical);

  //use left handed trapiziod function to calculate the water level threat for floods
  //risk begins at 3 cm and saturates (always 1.0 at this point or below) from 1.5cm (risk increases as water level increases)
  //the smaller the distance, the closer the water level to the sensor which means increased water level
  float waterLevelFloodRisk = leftHandedTrapizoid(waterLevel, waterLevelWarning, waterLevelCritical);

  //use left handed trapiziod function to calculate the humidity risk for floods, wildfires and drought
  //risk begins at 65.0% humidity and saturates (always 1.0 at this point or below) at 40% humidity
  float humidityRisk = leftHandedTrapizoid(humidity, humidityWarning, humidityCritical);

  //use left handed trapiziod function to calculate the soil moisture level risk for wildfires and drought
  //risk begins at a soil moisture level of 700.0 and saturates (always 1.0 at this point or below) at 300.0 soil moisture level
  float soilMoistureRisk = leftHandedTrapizoid(float(soilMoisture), soilMoistureWarning, soilMoistureCritical);


  //now calculate risk index for each disaster using Mamdani-Style Fuzzy Inference
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

  //see if there has been a environmental status override from the node RED dashboard, if so set packets correspondingly and sets disaster type
  if(environmentStateOverride != -1){
    //force normal state
    if(environmentStateOverride == 0){
      calculatedSleepDurationPacket.sleepDuration = normalSleepDuration;
      mitigationTransmissionCommand.activateFloodgate = false;
      mitigationTransmissionCommand.activateIrrigation = false;
      mitigationTransmissionCommand.communicationCheck = 0xAA;
      disasterType = 0;

      //create a data history object
      createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 0, 0);
      return 0;
    }
    //force Wildfire Warning
    else if(environmentStateOverride == 1){
      calculatedSleepDurationPacket.sleepDuration = warningSleepDuration;
      mitigationTransmissionCommand.activateFloodgate = false;
      mitigationTransmissionCommand.activateIrrigation = true;
      mitigationTransmissionCommand.communicationCheck = 0xAA;
      disasterType = 1;

      //create a data history object
      createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 1, 1);

      return 1;
    }
    //force flood Warning
    else if(environmentStateOverride == 2){
      calculatedSleepDurationPacket.sleepDuration = warningSleepDuration;
      mitigationTransmissionCommand.activateFloodgate = true;
      mitigationTransmissionCommand.activateIrrigation = false;
      mitigationTransmissionCommand.communicationCheck = 0xAA;
      disasterType = 2;

      //create a data history object
      createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 1, 2);

      return 1;
    }
    //force drought Warning
    else if(environmentStateOverride == 3){
      calculatedSleepDurationPacket.sleepDuration = warningSleepDuration;
      mitigationTransmissionCommand.activateFloodgate = false;
      mitigationTransmissionCommand.activateIrrigation = true;
      mitigationTransmissionCommand.communicationCheck = 0xAA;
      disasterType = 3;

      //create a data history object
      createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 1, 3);

      return 1;
    }
    //force Wildfire Critical
    else if(environmentStateOverride == 4){
      calculatedSleepDurationPacket.sleepDuration = criticalSleepDuration;
      mitigationTransmissionCommand.activateFloodgate = false;
      mitigationTransmissionCommand.activateIrrigation = false;
      mitigationTransmissionCommand.communicationCheck = 0xAA;
      disasterType = 1;

      //create a data history object
      createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 2, 1);

      return 2;
    }
    //focre Flood Critical
    else if(environmentStateOverride == 5){
      calculatedSleepDurationPacket.sleepDuration = criticalSleepDuration;
      mitigationTransmissionCommand.activateFloodgate = true;
      mitigationTransmissionCommand.activateIrrigation = false;
      mitigationTransmissionCommand.communicationCheck = 0xAA;
      disasterType = 2;

      //create a data history object
      createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 2, 2);

      return 2;
    }

  }


  //defuzzification - converting the calculated infered risk indexes into environment status (0: Normal, 1: WARNING, 2: CRITICAL)
  //also create priorities - wildfire, flood then drought
  //calculate mitigation actions and sleep duration

  //most critical - active wildfire
  //30 seconds Sensor Node sleep, close the floodgate and irrigation system for safety, set environment status to 2 (CRITICAL)
  if(wildfireRisk >= 0.7){
    calculatedSleepDurationPacket.sleepDuration = criticalSleepDuration;
    mitigationTransmissionCommand.activateFloodgate = false;
    mitigationTransmissionCommand.activateIrrigation = false;
    mitigationTransmissionCommand.communicationCheck = 0xAA;
    disasterType = 1;

    //create a data history object
    createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 2, 1);

    return 2;
  }

  //next critical - active flood
  //30 seconds Sensor Node sleep, open the floodgate and close irrigation system, set environment status to 2 (CRITICAL)
  else if(floodRisk >= 0.7){
    calculatedSleepDurationPacket.sleepDuration = criticalSleepDuration;
    mitigationTransmissionCommand.activateFloodgate = true;
    mitigationTransmissionCommand.activateIrrigation = false;
    mitigationTransmissionCommand.communicationCheck = 0xAA;
    disasterType = 2;

    //create a data history object
    createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 2, 2);

    return 2;
  }

  //wildfire warning
  //60 seconds Sensor Node sleep, close the floodgate and activate irrigation system, set environment status to 1 (WARNING)
  else if(wildfireRisk >= 0.35){
    calculatedSleepDurationPacket.sleepDuration = warningSleepDuration;
    mitigationTransmissionCommand.activateFloodgate = false;
    mitigationTransmissionCommand.activateIrrigation = true;
    mitigationTransmissionCommand.communicationCheck = 0xAA;
    disasterType = 1;

    //create a data history object
    createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 1, 1);

    return 1;
  }

  //flood warning
  //60 seconds Sensor Node sleep, open the floodgate and deactivate irrigation system, set environment status to 1 (WARNING)
  else if(floodRisk >= 0.35){
    calculatedSleepDurationPacket.sleepDuration = warningSleepDuration;
    mitigationTransmissionCommand.activateFloodgate = true;
    mitigationTransmissionCommand.activateIrrigation = false;
    mitigationTransmissionCommand.communicationCheck = 0xAA;
    disasterType = 2;

    //create a data history object
    createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 1, 2);

    return 1;
  }

  //drought warning
  //60 seconds Sensor Node sleep, close the floodgate and activate irrigation system, set environment status to 1 (WARNING)
  else if(droughtRisk >= 0.35){
    calculatedSleepDurationPacket.sleepDuration = warningSleepDuration;
    mitigationTransmissionCommand.activateFloodgate = false;
    mitigationTransmissionCommand.activateIrrigation = true;
    mitigationTransmissionCommand.communicationCheck = 0xAA;
    disasterType = 3;

    //create a data history object
    createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 1, 3);

    return 1;
  }

  //normal conditions now
  //300 seconds Sensor Node sleep (5 minutes), close the floodgate and deactivate irrigation system, set environment status to 0 (Normal)
  else{
    calculatedSleepDurationPacket.sleepDuration = normalSleepDuration;
    mitigationTransmissionCommand.activateFloodgate = false;
    mitigationTransmissionCommand.activateIrrigation = false;
    mitigationTransmissionCommand.communicationCheck = 0xAA;
    disasterType = 0;

    //create a data history object
    createDataHistoryObject(temperature, humidity, waterLevel, soilMoisture, 0, 0);

    return 0;
  }

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




//function that converts sensor data, turns it into a dataHistoryObj and appends it to the historyLog 
void createDataHistoryObject(float temperature, float humidity, float waterLevel, int soilMoisture, int environmentStatus, int disasterType){
  //write the data at the current index position of the historyLog array
  historyLog[currentFilledHistoryIndex].timestamp = getTimestamp();
  historyLog[currentFilledHistoryIndex].temperature = temperature;
  historyLog[currentFilledHistoryIndex].humidity = humidity;
  historyLog[currentFilledHistoryIndex].waterLevel = waterLevel;
  historyLog[currentFilledHistoryIndex].soilMoisture = soilMoisture;
  historyLog[currentFilledHistoryIndex].environmentStatus = environmentStatus;
  historyLog[currentFilledHistoryIndex].disasterType = disasterType;

  //increment the current index position pointer to the next slot, or wrap around if the maximum 15 slots are filled
  currentFilledHistoryIndex = (currentFilledHistoryIndex + 1) % max_entries;

  //increment the totalHistoryEntries variable
  if(totalHistoryEntries < max_entries){
    totalHistoryEntries++;
  }

}


//function that calculates the timestamp based on the microcontroller's uptime
String getTimestamp(){
  //the microcontroller clock works in milliseconds, so convert it into seconds, then calculate minutes, hours and days
  //calculate seconds
  unsigned long totalSeconds = millis() / 1000;

  //now calculate seconds, minutes, hours and days
  int seconds = totalSeconds % 60;
  int minutes = (totalSeconds / 60) % 60;
  int hours = (totalSeconds / 3600) %  24;
  int days = totalSeconds / 86400;

  //now create a string for the timestamp - format it like: "Day D, HH:MM:SS" e.g. "Day 0, 02:15:30"
  String timestampString = "Day " + String(days) + ", ";

  //hours - add padding if required
  if(hours < 10){
    timestampString += "0";
  }
  timestampString += String(hours) + ":";

  //minutes - add padding if required
  if(minutes < 10){
    timestampString += "0";
  }
  timestampString += String(minutes) + ":";

  //seconds - add padding if required
  if(seconds < 10){
    timestampString += "0";
  }
  timestampString += String(seconds);


  //now return the timestamp string
  return timestampString;

}


//function to retrieve the latest sensor reading history object stored used for the local webpage and captive portal
dataHistoryObj getLatestReading(){
  //if no sensor readings have been sent by esp-now, then return a placeholder
  if(totalHistoryEntries == 0){
    return {"No data", 0.0, 0.0, 0.0, 0, 0, 0};
  }

  //otherwise calculate and retrieve most recent data entry
  //last item = most recent: queue - First In First Out (FIFO);
  int lastIndex = (currentFilledHistoryIndex - 1 + max_entries) % max_entries;
  return historyLog[lastIndex];  
}


//main web dashboard page
void getIndex(){

  //calculate background colour based on environment status recorded
  //deep red for critical, orangeish-yellow for warning and pale light green for normal
  String backgroundColour = (environmentStatus == 2) ? "#8C2939" : ((environmentStatus == 1 ) ? "#ffebbd" : "#D2FACD");

  //calculate text colour based on environment status recorded
  //white for critical, black for warning and normal 
  String textColour = (environmentStatus == 2) ? "#FFFFFF" : "#000000";

  //concatinate strings representing html code to make up the web dashboard
  //make the web dashboard responsive to different devices, and refresh every 2 minutes (120 seconds)
  String html = "<html><head><meta http-equiv='refresh' content='120' name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta charset='UTF-8'>";

  //concatinate styling - similar to 'stylesheet'
  //body style - background colour and text colour is dynamic based on environment status
  html += "<style>body{font-family: sans-serif; text-align: center; padding: 20px; background:" + backgroundColour + "; color:" + textColour + ";}";
  //most recent sensor reading and environment status style
  html += ".currentSensorReadingSection{background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); max-width: 400px; margin: 20px auto; color: black;}";
  //button styling - blue button with white text
  html += ".button{display: inline-block; padding: 12px 24px; background: #007bff; color: white; border-radius: 5px; margin-top: 15px; font-weight: bold; max-width: 400px;}</style></head>";

  //main body of webpage
  //display header
  html += "<body><h1>A.E.R.O. web dashboard!</h1>";

  //display most recent sensor reading data
  html += "<div class='currentSensorReadingSection'>";
  html += "<h3>Most Recent Sensor Reading</h3>";
  //display environment condition and disaster type corresponding text first
  html += checkConditionAndGetText();
  html += "<p><strong>Timestamp: </strong><br>" + String(getLatestReading().timestamp) + "</p>";
  html += "<p><strong>Temperature: </strong><br>" + String(getLatestReading().temperature) + "°C</p>";
  html += "<p><strong>Humidity: </strong><br>" + String(getLatestReading().humidity) + "%</p>";
  html += "<p><strong>Water Level: </strong><br>" + String(getLatestReading().waterLevel) + "cm</p>";
  html += "<p><strong>Soil Moisture Level: </strong><br>" + String(getLatestReading().soilMoisture) + "</p>";
  html += "</div>";


  //add the button to go to the sensor reading history page
  html += "<br><a href='/history' class='button'>View Past Sensor Reading History</a>";

  //close the body and html page
  html += "</body></html>";


  //now send the webpage to the browser
  server.send(200, "text/html", html);
}



//the sensor history log web dashboard page
void getHistory(){
  //concatinate strings representing html code to make up the web dashboard
  //make the web dashboard responsive to different devices, and refresh every 2 minutes (120 seconds)
  String html = "<html><head><meta http-equiv='refresh' content='120' name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta charset='UTF-8'>";

  //stylesheet
  //body style - background colour is light grey
  html += "<style>body{font-family: sans-serif; padding: 20px; background: #FAFAFA; text-align: center;}";
  //table styling
  html += "table{width: 100%; max-width: 600px; margin: 20px auto; border-collapse: collapse; background: white}";
  html += "th, td{padding: 10px; border: 1px solid #ddd; text-align: center} th{background: #007bff; color: white;} </style>";

  //main body of webpage
  //display header
  html += "<body>";
  html += "<h1>Past Sensor Reading History</h1>";

  //button to go back to dashboard
  html += "<a href='/' class='button'>Back to homepage</a><br>";

  //start the table grid layout and name the columns
  html += "<table><tr><th>Time</th><th>Temperature</th><th>Humidity</th><th>Water Level</th><th>Soil Moisture</th><th>Environment evaluation</th></tr>";
  
  //now iterate over sensor readings from newest to oldest entry (newest at top)
  for(int i = totalHistoryEntries - 1; i >= 0; i--){
    int targetIndex = (currentFilledHistoryIndex - totalHistoryEntries + i + max_entries) % max_entries;
    dataHistoryObj currentEntry = historyLog[targetIndex];

    //map the row colour to the entry record's environmental status
    String rowColour = (currentEntry.environmentStatus == 2) ? "#8C2939" : ((currentEntry.environmentStatus == 1 ) ? "#ffebbd" : "#FFFFFF");
    //now evaluate the row text colour - white for critical dark red, otherwise black 
    String rowTextColour = (currentEntry.environmentStatus == 2) ? "#FFFFFF" : "#000000"; 
    
    //dictonaries to map the environment status and disaster type integers to words
    String statusString[] = {"Normal", "Warning", "Critical"};
    String disasterString[] = {"Normal", "Wildfire", "Flood", "Drought"};
    
    //now add the data to the table row with colour styling
    html += "<tr style='background:" + rowColour + "; color:" + rowTextColour + ";'>";
    html += "<td>" + currentEntry.timestamp + "</td>";
    html += "<td>" + String(currentEntry.temperature) + "</td>";
    html += "<td>" + String(currentEntry.humidity) + "</td>";
    html += "<td>" + String(currentEntry.waterLevel) + "</td>";
    html += "<td>" + String(currentEntry.soilMoisture) + "</td>";
    html += "<td>" + String(statusString[currentEntry.environmentStatus]) + " - " + String(disasterString[currentEntry.disasterType]) + "</td>";
    html += "</tr>";
  }

  //close the table, body and html page
  html += "</table></body></html>";

  //now send the webpage to the browser
  server.send(200, "text/html", html);
}



//text to display on the web dashboard when there is a warning of a wildfire
String getWarningWildfireText(){
  //golden orange text color
  String text = "<div style='color:#E8AA0E; padding:15px; border-radius: 5px; margin: 15px auto; max-width: 400px; font-weight:bold;'>"
          "<h3 style='margin:0; text-align:center;'> WILDFIRE WARNING: </h3>"
          "<ul style='margin:10px 0 0 20px;'><li>Pack emergency supplies</li><li>Clear dry debris from gutters</li><li>Don't have BBQs or burn bonfires</li><li>Monitor local news and alerts</li></ul>"
          "</div>";
  return text;
}

//text to display on the web dashboard when there is a warning of a flood
String getWarningFloodText(){
  //golden orange text color
  String text = "<div style='color:#E8AA0E; padding:15px; border-radius: 5px; margin: 15px auto; max-width: 400px; font-weight:bold;'>"
          "<h3 style='margin:0; text-align:center;'> FLOOD WARNING: </h3>"
          "<ul style='margin:10px 0 0 20px;'><li>Move critical electrical items upstairs</li><li>Turn off primary utility valves</li></ul>"
          "</div>";
  return text;
}

//text to display on the web dashboard when there is a warning of a drought
String getWarningDroughtText(){
  //golden orange text color
  String text = "<div style='color:#E8AA0E; padding:15px; border-radius: 5px; margin: 15px auto; max-width: 400px; font-weight:bold;'>"
          "<h3 style='margin:0; text-align:center;'> DROUGHT WARNING: </h3>"
          "<ul style='margin:10px 0 0 20px;'><li>Strict domestic water restrictions are active</li><li>Hosepipe ban effective</li></ul>"
          "</div>";
  return text;
}

//text to display on the web dashboard when wildfire is critical
String getCriticalWildfireText(){
  //deep red text color
  String text = "<div style='color:#B02C15; padding:15px; border-radius: 5px; margin: 15px auto; max-width: 400px; font-weight:bold;'>"
          "<h3 style='margin:0; text-align:center;'> CRITICAL WILDFIRE: </h3>"
          "<ul style='margin:10px 0 0 20px;'><li><strong>Evacuate Immediately!</strong></li><li>Follow local emergency routes</li><li>Do not delay.</li></ul>"
          "</div>";
  return text;
}

//text to display on the web dashboard when flood is critical
String getCriticalFloodText(){
  //deep-red text color
  String text = "<div style='color:#B02C15; padding:15px; border-radius: 5px; margin: 15px auto; max-width: 400px; font-weight:bold;'>"
          "<h3 style='margin:0; text-align:center;'> CRITICAL FLOOD: </h3>"
          "<ul style='margin:10px 0 0 20px;'><li>Move to the highest floor you can or roof immediately</li><li>Avoid driving or walking through moving water</li></ul>"
          "</div>";
  return text;
}

//text to display on the web dashboard when conditions are normal
String getNormalText(){
  //green text color
  String text = "<div style='color:#53CF62; padding:15px; border-radius: 5px; margin: 15px auto; max-width: 400px; font-weight:bold;'>"
          "<h3 style='margin:0; text-align:center;'> Normal Conditions </h3>"
          "</div>";
  return text;
}


//function that retrieves the required warning or critical text to display on the dashboard
String checkConditionAndGetText(){
  //normal conditions, do not need to return any special text
  if(environmentStatus == 0){
    return getNormalText();
  }

  //warning states
  if(environmentStatus == 1){
    if(disasterType == 1){
      return getWarningWildfireText();
    }
    else if(disasterType == 2){
      return getWarningFloodText();
    }
    else if(disasterType == 3){
      return getWarningDroughtText();
    }
  }

  //critical states
  if(environmentStatus == 2){
    if(disasterType == 1){
      return getCriticalWildfireText();
    }
    else if(disasterType == 2){
      return getCriticalFloodText();
    }
  }

  //safe fallback string
  return "";
}

