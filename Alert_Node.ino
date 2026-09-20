
//library for LCD screen
#include <LiquidCrystal_I2C.h>

//create a LCD object
LiquidCrystal_I2C lcdScreen(0x27, 16, 2);

//create variable storing current and previous environment status - (0: good - green, 1: warning - amber, 2: warning - red)
int environmentStatus = 0;
int previousStatus = -1;



void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);

  //initialise the LCD screen and its backlight
  lcdScreen.init();
  lcdScreen.backlight();

  //display normal message 
  lcdScreen.clear();
  //displayNormalText();

}

void loop() {
  // put your main code here, to run repeatedly:

  //check if the environment status has changed and update accordingly
  updateStatus();

  //randomly go through the three states - 0: normal (green), 1: WARNING (yellow), and 2: CRITICAL (red)
  environmentStatus = random(0, 3);
  Serial.print("Environment Status: ");
  Serial.print(environmentStatus);
  Serial.print(" | ");
  Serial.print("Previous Status: ");
  Serial.println(previousStatus);
  delay(2000);

}


//function that checks if environment status has changed, if it has, act accordingly and update previousStatus
void updateStatus(){

  //status changed to normal
  if(environmentStatus == 0 && previousStatus !=0){
    lcdScreen.clear();
    displayNormalText();
    previousStatus = 0;
  } 

  //status changed to WARNING
  if(environmentStatus == 1 && previousStatus !=1){
    lcdScreen.clear();
    displayWarningText();
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



