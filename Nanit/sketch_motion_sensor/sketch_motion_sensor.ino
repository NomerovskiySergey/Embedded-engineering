#include <NanitLib.h>
/*
Write an alarm system program. 
The purpose of the program is that in the loop() function the LED blinks once every second,
and if the sensor detects motion, the piezo buzzer should be activated and 
the LED blinking interval should be changed.
*/

byte led_pin = P5_4;
byte buzzer_pin = P5_3;
byte motion_pin = P5_1;
byte resetBtn_pin = P5_2;

bool isAlarm = false;

void setup() {
  Nanit_Base_Start();
  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(motion_pin), interruptCallback, RISING);

  pinMode(led_pin, OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(resetBtn_pin, INPUT_PULLUP);
}

void loop() {
  bool resetBtn = !digitalRead(resetBtn_pin);

  if (resetBtn)
    resetAlarm();

  if (!isAlarm) {
    ledBlincking(led_pin, 2000);
  } else {
    ledBlincking(led_pin, 500);
  }
}

void ledBlincking(byte pin, int del) {
  digitalWrite(pin, HIGH);
  delay(del);
  digitalWrite(pin, LOW);
  delay(del);
}

void resetAlarm() {
  noTone(buzzer_pin);
  isAlarm = false;
}

void interruptCallback() {
  Serial.println("Motion detected!");
  isAlarm = true;
  tone(buzzer_pin, 255);
}
