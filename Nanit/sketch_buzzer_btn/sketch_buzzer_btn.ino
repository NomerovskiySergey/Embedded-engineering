#include <NanitLib.h>
#include "Btn.h"
#include "Buzzer.h"
#include "Potentiometer.h"

Btn btn(P4_2);
Buzzer buz(P4_3);
Potentiometer pot(P4_4);

void setup() {
  Nanit_Base_Start();
  btn.Init();
  buz.Init();
  pot.Init();
}

void loop() {
  bool btnState = btn.GetState();

  if (btnState) {
    buz.On();
  } else {
    buz.Off();
  }

  /*
  int potVal = pot.Read();
  buz.Tone(potValue);
  delay(500);
  */
}
