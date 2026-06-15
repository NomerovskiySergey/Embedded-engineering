#include <NanitLib.h>

/*
Write a program that uses a sound sensor to detect claps. 
Each new clap should turn on the next LED in sequence. The system should support between 2 and 6 LEDs.
*/

byte soundAnalog_pin = P5_2;
byte led1_pin = P4_1;
byte led2_pin = P4_2;
byte led3_pin = P4_3;
byte led4_pin = P4_4;

int clapCounter;


byte ledLine [] = {led1_pin, led2_pin, led3_pin, led4_pin};

void setup() {
  Nanit_Base_Start();

  pinMode(soundAnalog_pin, INPUT);
  pinMode(led1_pin, OUTPUT);
  pinMode(led2_pin, OUTPUT);
  pinMode(led3_pin, OUTPUT);
  pinMode(led4_pin, OUTPUT);

  clapCounter = 0;

  Serial.begin(9600);
}

void loop() {
  int soundValue = analogRead(soundAnalog_pin);
    
  if(soundValue > 200){
    delay(1000);
    digitalWrite(ledLine[clapCounter], HIGH);
    clapCounter ++;
  }

  if(clapCounter == 5){
    clapCounter = 0;

    for(int i = 0; i < 4; i++){
      digitalWrite(ledLine[i], LOW);
    }
  }  
}
