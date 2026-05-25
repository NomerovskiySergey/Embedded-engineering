/*
 Project 4
 Btn controlled led
*/
int bntPin = 7;
int ledPin = 11;

int btnVal;

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(bntPin, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  btnVal = digitalRead(bntPin);

  Serial.print("Btn:");
  Serial.print(btnVal);
  Serial.println("");

  if (btnVal == LOW) {
    digitalWrite(ledPin, LOW);
  } else {
    digitalWrite(ledPin, HIGH);
  }
}