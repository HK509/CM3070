
//ultrasonic distance sensor pins - trigger and echo
const int TRIG_PIN = D5;
const int  ECHO_PIN = D6;

//ultrasonic sensor reading storage variable - water level distance
float waterLevel_distance;

void setup() {
  // put your setup code here, to run once:

  //begin serial monitor
  Serial.begin(9600);

  //configure the ultrasonic sensor pins: trigger pin to output mode and echo pin to input
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

}

void loop() {
  // put your main code here, to run repeatedly:

  //calculate water level by calling the measureWaterLevel function to read ultrasonic distance sensor
  waterLevel_distance = measureWaterLevel();
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
  
  Serial.print("Sensor Measured: ");
  Serial.print(calculatedDistance);
  Serial.println(" cm");

  return calculatedDistance;
}