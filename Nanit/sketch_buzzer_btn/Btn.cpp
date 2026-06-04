#include "Arduino.h"
#include "Btn.h"

Btn::Btn(int pin) {
  _pin = pin;
}

void Btn::Init() {
  pinMode(_pin, INPUT_PULLUP);
}

bool Btn::GetState() {
  return digitalRead(_pin);
}
