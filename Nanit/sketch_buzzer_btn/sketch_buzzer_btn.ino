#include <NanitLib.h>
#include "Btn.h"
#include "Buzzer.h"
#include "Potentiometer.h"

Btn btn(P4_2);
Buzzer buz(P4_3);
Potentiometer pot(P4_1);

void setup() {
  Nanit_Base_Start();
  btn.Init();
  buz.Init();
  pot.Init();

  Serial.begin(9600);
}

void loop() {
  //bool btnState = btn.GetState();

  // if (!btnState) {
  //   buz.On();
  // } else {
  //   buz.Off();
  // }

  
  int potVal = pot.Read();
  Serial.println(potVal);

  buz.Tone(potVal);
  delay(500);
  
}
