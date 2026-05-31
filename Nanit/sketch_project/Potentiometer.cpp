#include "Arduino.h"
#include "Potentiometer.h"

Potentiometer::Potentiometer(int pin) {
  _pin = pin;
}

void Potentiometer::Init() {
  pinMode(_pin, INPUT);
}

int Potentiometer::Read() {
  return analogRead(_pin);
}
