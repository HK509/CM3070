
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



void setup() {
  // put your setup code here, to run once:

  //begin serial monitor
  Serial.begin(9600);

  //define servo pin - floodgate, and min pulse width (500 as 0°) and max pulse width (2500°)
  floodgate.attach(D2, 500, 2500);

  //initialise water pump relay pin as output, and set it as off - HIGH because it is a low level trigger relay
  pinMode(WATER_PUMP_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_PIN, HIGH);

}

void loop() {
  // put your main code here, to run repeatedly:

  activateFloodgate();
  delay(1000);
  deactivateFloodgate();
  delay(2000);
  activateIrrigation();
  delay(1000);
  deactivateIrrigation();
  delay(2000);
  

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
