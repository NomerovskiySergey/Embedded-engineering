#include "Arduino.h"
#include "Buzzer.h"

Buzzer::Buzzer(int pin) {
  _pin = pin;
}

void Buzzer::Init() {
  pinMode(_pin, OUTPUT);
}

void Buzzer::On() {
  return digitalWrite(_pin, HIGH);
}

void Buzzer::Off() {
  return digitalWrite(_pin, LOW);
}

void Buzzer::Tone(int value) {
  tone(_pin, value);
}
