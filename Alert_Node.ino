
//library for LCD screen
#include <LiquidCrystal_I2C.h>

//create a LCD object
LiquidCrystal_I2C lcdScreen(0x27, 16, 2);



void setup() {
  // put your setup code here, to run once:

  //initialise the LCD screen and its backlight
  lcdScreen.init();
  lcdScreen.backlight(); 

  //display testing text on LCD 
  lcdScreen.clear();                 
  lcdScreen.setCursor(0, 0);       
  lcdScreen.print("Alert Node"); 

}

void loop() {
  // put your main code here, to run repeatedly:


  
}
