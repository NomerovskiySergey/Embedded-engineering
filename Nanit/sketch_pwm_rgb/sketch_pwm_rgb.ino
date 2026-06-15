#include <NanitLib.h>

/*
Create two functions: one for gradually increasing the brightness of the red, green, and blue LEDs, 
and another for gradually decreasing their brightness. 
The colors should fade in and fade out smoothly, one after another.
*/

byte red_pin = P4_2;
byte green_pin = P4_3;
byte blue_pin = P4_4;

void setup() {
  Nanit_Base_Start();

  pinMode(red_pin, OUTPUT);
  pinMode(green_pin, OUTPUT);
  pinMode(blue_pin, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  led_on(red_pin);
  led_on(green_pin);
  led_on(blue_pin);
  delay(500);
  led_off(red_pin);
  led_off(green_pin);
  led_off(blue_pin);
}

void led_on(byte pin) {
  for (int i = 0; i < 255; i++) {
    analogWrite(pin, i);
    delay(30);
  }
}

void led_off(byte pin) {
  for (int i = 255; i > 0; i--) {
    analogWrite(pin, i);
    delay(30);
  }
}