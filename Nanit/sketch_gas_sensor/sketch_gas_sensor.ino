#include <NanitLib.h>

byte gasSensor_pin = P5_2;
byte greenLed_pin  = P5_1;
byte redLed_pin    = P5_4;

void setup() {
  Nanit_Base_Start();
  Serial.begin(9600);

  pinMode(gasSensor_pin, INPUT);
  pinMode(greenLed_pin, OUTPUT);
  pinMode(redLed_pin, OUTPUT);
}

void loop() {
  int gas = analogRead(gasSensor_pin);

  Serial.println(gas);

  if(gas > 500){
    digitalWrite(redLed_pin, HIGH);
    digitalWrite(greenLed_pin, LOW);
  }
  else{
    digitalWrite(redLed_pin, LOW);
    digitalWrite(greenLed_pin, HIGH);
  }
}
