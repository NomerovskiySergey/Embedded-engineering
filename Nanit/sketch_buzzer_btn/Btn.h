#pragma once

class Btn{
  public: 
    Btn(int pin);
    void Init();
    bool GetState();
  
  private:
    int _pin;
};