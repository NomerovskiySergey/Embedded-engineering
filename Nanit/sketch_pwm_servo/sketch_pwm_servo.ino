#include <NanitLib.h>
#include <servo.h>

/*
Write a program where four buttons control a servomotor, and each button moves it to a specific angle.
*/

byte servo_pin    = P5_4;
byte angle0_pin   = P4_1;
byte angle90_pin  = P4_2;
byte angle180_pin = P4_3;
byte angle360_pin = P4_4;

Servo servo;

void setup() {
  Nanit_Base_Start();

  servo.attach(servo_pin);
  servo.write(0);

  pinMode(angle0_pin,   INPUT_PULLUP);
  pinMode(angle90_pin,  INPUT_PULLUP);
  pinMode(angle180_pin, INPUT_PULLUP);
  pinMode(angle360_pin, INPUT_PULLUP);
}

void loop() {
  
}
