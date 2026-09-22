

//libraries for ESP-NOW communication
#include <ESP8266WiFi.h>
#include <espnow.h>

//library for servo motor
#include <Servo.h>


//declare water pump relay pin - irrigation system 
const int WATER_PUMP_PIN = D5;

//create a servo object - floodgate
Servo floodgate;


//create and initialise a variable to store floodgate status, and its position
bool floodgateActive = false;
int floodgate_pos = 0;

//create and initialise a variable to store irrigation system status
bool irrigationActive = false;


//define a mitigation command packet that will be used for communication to the mitigation node
struct mitigationCommandPacket{
  bool activateFloodgate;
  bool activateIrrigation;
  byte communicationCheck;
};

//create a variable of the calculated mitigation action command packet data structure defined
mitigationCommandPacket mitigationTransmissionRecieved;



// Automatically triggers whenever Alert Node (Node 3) transmits a message packet
void onDataRecv(uint8_t * mac, uint8_t *incomingByte, uint8_t len) {
  
  // Unpack incoming bytes back into structured reading values
  memcpy(&mitigationTransmissionRecieved, incomingByte, sizeof(mitigationTransmissionRecieved));
  
  // Print results to serial monitor
  Serial.print("Data Packet Recieved from Alert Node (Node 3)");
  Serial.print("Activate Floodgate: "); Serial.print(mitigationTransmissionRecieved.activateFloodgate);
  Serial.print(" | ");
  Serial.print("Activate Irrigation: "); Serial.print(mitigationTransmissionRecieved.activateIrrigation);
  Serial.print(" | ");
  Serial.print("communicationCheck: "); Serial.println(mitigationTransmissionRecieved.communicationCheck);

  //now carry out commands
  //deactivate irrigation and and activate floodgate - Warning and Critical Flood
  if(mitigationTransmissionRecieved.activateFloodgate == true &&  mitigationTransmissionRecieved.activateIrrigation == false){
    deactivateIrrigation();
    activateFloodgate();
  }
  //deactivate floodgate and and and activate irrigation - Warning Wildfire and Drought
  else if(mitigationTransmissionRecieved.activateFloodgate == false &&  mitigationTransmissionRecieved.activateIrrigation == true){
    deactivateFloodgate();
    activateIrrigation();
  }
  //deactivate both floodgate and irrigation system - Normal, Critical Wildfire
  else{
    deactivateFloodgate();
    deactivateIrrigation();
  }
}


void setup() {
  // put your setup code here, to run once:

  //begin serial monitor
  Serial.begin(115200);
  
  //wake up the external built-in antenna in the esp8266 and set it into WiFi station mode which is required for ESP-NOW
  WiFi.forceSleepWake();
  delay(10);
  WiFi.mode(WIFI_STA);

  //define servo pin - floodgate, and min pulse width (500 as 0°) and max pulse width (2500°)
  floodgate.attach(D2, 500, 2500);

  //initialise water pump relay pin as output, and set it as off - HIGH because it is a low level trigger relay
  pinMode(WATER_PUMP_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_PIN, HIGH);


  //check ESP-NOW connection if failed, add message to serial monitor
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed");
  }

  //set device role as reciever (slave) 
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  //register the transmission callback function defined
  esp_now_register_recv_cb(onDataRecv);

}

void loop() {
  // put your main code here, to run repeatedly:

  //activateFloodgate();
  //delay(1000);
  //deactivateFloodgate();
  //delay(2000);
  //activateIrrigation();
  //delay(1000);
  //deactivateIrrigation();
  //delay(2000);
  

}


//function that activates/ opens the floodgate - servo rotates from 0° to 180°
void activateFloodgate(){
  if(floodgateActive == false){
    for(floodgate_pos = 0; floodgate_pos <= 180; floodgate_pos += 1){
      floodgate.write(floodgate_pos);
      delay(10);
    }
  }

  floodgateActive = true;

  Serial.print("Floodgate Active: ");
  Serial.println(floodgateActive);
}



//function that deactivates/ closes the floodgate - servo rotates from 180° to 0°
void deactivateFloodgate(){
  if(floodgateActive == true){
    for(floodgate_pos = 180; floodgate_pos >= 0; floodgate_pos -= 1){
      floodgate.write(floodgate_pos);
      delay(10);
    }
  }

  floodgateActive = false;

  Serial.print("Floodgate Active: ");
  Serial.println(floodgateActive);
}



//function that activates/ starts the irigation system - pump turns on
void activateIrrigation(){
  if(irrigationActive == false){

    //temporarily remove floodgate when pump is on (both fight over power)
    floodgate.detach();
    delay(500);
    digitalWrite(WATER_PUMP_PIN, LOW);
  }

  irrigationActive = true;

  Serial.print("Irrigation Active: ");
  Serial.println(irrigationActive);
}



//function that deactivates/ stops the irigation system - pump turns off
void deactivateIrrigation(){
  if(irrigationActive == true){
    digitalWrite(WATER_PUMP_PIN, HIGH);
  }

  irrigationActive = false;

  //reatach floodgate now pump is off
  floodgate.attach(D2, 500, 2500);

  Serial.print("Irrigation Active: ");
  Serial.println(irrigationActive);
}
