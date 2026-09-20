
//library for servo motor
#include <Servo.h>


//create a servo object - floodgate
Servo floodgate;


//create and initialise a variable to store floodgate status, and its position
bool floodgateActive = false;
int floodgate_pos = 0;



void setup() {
  // put your setup code here, to run once:

  //begin serial monitor
  Serial.begin(9600);

  //define servo pin - floodgate
  floodgate.attach(D2);

}

void loop() {
  // put your main code here, to run repeatedly:

  activateFloodgate();
  delay(500);
  deactivateFloodgate();
  delay(500);

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
  if(floodgateActive == false){
    for(floodgate_pos = 180; floodgate_pos >= 0; floodgate_pos -= 1){
      floodgate.write(floodgate_pos);
      delay(10);
    }
  }

  floodgateActive = false;

  Serial.print("Floodgate Active: ");
  Serial.println(floodgateActive);
}
