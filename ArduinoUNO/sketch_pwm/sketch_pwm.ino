/*
-- Without potentiometr

int ledPin = 10;

void setup() {
  pinMode(ledPin, OUTPUT);
}

void loop() {
  for (int value = 0 ; value < 255; value=value+1){
    analogWrite(ledPin, value);
    delay(5);
  }
  for (int value = 255; value >0; value=value-1){
    analogWrite(ledPin, value);
    delay(5);
  }
}
*/

// With potentiometr

int ledPin = 10;
int potPin = A0;

int potValue = 0;         
int pwmValue = 0;

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600); 
}

void loop() {
  potValue = analogRead(potPin);                    
  pwmValue = map(potValue, 0, 1023, 0, 255);        
  analogWrite(ledPin, pwmValue);      

  Serial.print("Analog: ");
  Serial.print(potValue);
  Serial.print(" -> PWM: ");
  Serial.println(pwmValue);

  delay(10);
}
