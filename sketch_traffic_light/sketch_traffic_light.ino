/*
 Project 4
 traffic light
*/
int redLed    = 10;
int yellowLed = 7;
int blueLed   = 4;


void setup()
{
  pinMode(redLed, OUTPUT);
  pinMode(yellowLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
}

void loop()
{
    digitalWrite(blueLed, HIGH);
    delay(5000);
    digitalWrite(blueLed, LOW); 

    for(int i=0;i<3;i++)
    {
        delay(500);
        digitalWrite(yellowLed, HIGH);
        delay(500);
        digitalWrite(yellowLed, LOW);
    } 

    delay(500);
    digitalWrite(redLed, HIGH);
    delay(5000);
    digitalWrite(redLed, LOW);
}