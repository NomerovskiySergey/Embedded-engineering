#pragma once

class Buzzer{
  public: 
    Buzzer(int pin);
    void Init();
    void On();
    void Off();
    void Tone(int value);
  
  private:
    int _pin;
}