#pragma once

class Potentiometer{
  public: 
    Potentiometer(int pin);
    void Init();
    int Read();
  
  private:
    int _pin;
};