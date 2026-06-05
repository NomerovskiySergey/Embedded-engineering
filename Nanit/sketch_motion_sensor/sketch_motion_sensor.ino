#include <NanitLib.h>
/*
Write an alarm system program. 
The purpose of the program is that in the loop() function the LED blinks once every second,
and if the sensor detects motion, the piezo buzzer should be activated and 
the LED blinking interval should be changed.
*/

byte led_pin      = P5_4;
byte buzzer_pin   = P5_3;
byte motion_pin   = P5_1;
byte resetBtn_pin = P5_2;

void setup() {
  Nanit_Base_Start();  
  Serial.begin(9600);  

  attachInterrupt(digitalPinToInterrupt(motion_pin), interruptCallback, RISING);

  pinMode(led_pin, OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(resetBtn_pin, INPUT_PULLUP);
}

void loop() {
  bool resetBtn = digitalRead(resetBtn_pin);

  if(resetBtn)
    resetAlarm();

  ledBlincking(led_pin, 1000);
}

void ledBlincking(byte pin, int del){
  digitalWrite(pin, HIGH);
  delay(del);
  digitalWrite(pin, LOW);
}

void resetAlarm(){
  ledBlincking(led_pin, 1000);
  digitalWrite(buzzer_pin, LOW);
}

void interruptCallback() {                    
  Serial.println("Motion detected!"); 
  ledBlincking(led_pin, 500);
  digitalWrite(buzzer_pin, HIGH);
}
