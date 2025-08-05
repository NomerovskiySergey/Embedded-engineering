/*
 Project 4
 Chasing effect cvds
*/
int startPin = 2;
int endPin   = 11;


void setup()
{
  for(int i = startPin; i <= endPin; i++){
    pinMode(i, OUTPUT);
  }
}

void loop()
{
    for(int i = startPin; i <= endPin; i++)
    {
        digitalWrite(i, HIGH);
        delay(100);
    } 

    for(int i = startPin; i <= endPin; i++)
    {
        digitalWrite(i, LOW);
        delay(100);
    } 
}