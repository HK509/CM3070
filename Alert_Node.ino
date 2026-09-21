
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
  int waterLevel;
  int soilMoisture;
};

//create a variable of the sensor reading packet that will be recieved from the Sensor Node (Node 1)
sensorReadingPacket sensorReadingsRecieved;


// Automatically triggers whenever Sensor Node (Node 1) transmits a message packet
void onDataRecv(uint8_t * mac, uint8_t *incomingByte, uint8_t len) {
  
  // Unpack incoming bytes back into structured reading values
  memcpy(&incomingData, incomingByte, sizeof(incomingData));
  
  // Print results to serial monitor
  Serial.print("Data Packet Recieved from Sensor Node (Node 1)")
  Serial.print("Temperature: "); Serial.print(sensorReadingsRecieved.temperature); Serial.print(" °C");
  Serial.print(" | ");
  Serial.print("Humidity: "); Serial.print(sensorReadingsRecieved.humidity); Serial.print(" %");
  Serial.print(" | ");
  Serial.print("Water Level Distance: "); Serial.print(sensorReadingsRecieved.waterLevel); Serial.print(" cm");
  Serial.print(" | ");
  Serial.print("Soil Moisture Level: "); Serial.println(sensorReadingsRecieved.soilMoisture);
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

  //set device role as reciever (slave) 
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  //register the transmission callback function defined
  esp_now_register_recv_cb(onDataRecv);

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



