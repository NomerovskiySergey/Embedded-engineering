#include <NanitLib.h>

byte red    = P3_2;
byte yellow = P3_3;
byte green  = P3_4;

void setup() {
  Nanit_Base_Start();

  pinMode(P3_2, OUTPUT);
  pinMode(P3_3, OUTPUT);
  pinMode(P3_4, OUTPUT);
}

void trafficLightSwitch(bool redSignal, bool yellowSignal, bool greenSignal, int del = 1000)
{
  digitalWrite(red, redSignal);
  digitalWrite(yellow, yellowSignal);
  digitalWrite(green, greenSignal);
  delay(del);
}

void loop() {
  trafficLightSwitch(HIGH, LOW, LOW);
  trafficLightSwitch(HIGH, HIGH, LOW);
  trafficLightSwitch(LOW, LOW, HIGH);
  trafficLightSwitch(LOW, LOW, LOW);
}

