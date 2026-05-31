#include <NanitLib.h>

byte ledPin = P3_2;

void setup() {
  Nanit_Base_Start();
  pinMode(ledPin, OUTPUT);
}

void loop() {
    digitalWrite(ledPin, HIGH);
    delay(1000);

    digitalWrite(ledPin, LOW);
    delay(1000);
}
