#include <NanitLib.h>

byte buzzer = P4_2;

// Notes
#define E4 330
#define F4 349
#define G4 392
#define A4 440
#define AS4 466
#define B4 494

#define C5 523
#define D5 587
#define E5 659
#define F5 698
#define G5 784
#define A5 880

#define REST 0

struct Note {
  int freq;
  int dur;
};

Note mario[] = {

  {E5,150},{E5,150},{REST,150},{E5,150},
  {REST,150},{C5,150},{E5,150},
  {G5,300},{REST,300},{G4,300},

  {C5,300},{REST,150},{G4,300},
  {REST,150},{E4,300},

  {A4,300},{B4,300},{AS4,150},{A4,300},

  {G4,200},{E5,200},{G5,200},
  {A5,300},{F5,150},{G5,150},

  {REST,150},

  {E5,300},{C5,150},{D5,150},{B4,300},

  {C5,300},{REST,150},{G4,300},
  {REST,150},{E4,300},

  {A4,300},{B4,300},{AS4,150},{A4,300},

  {G4,200},{E5,200},{G5,200},
  {A5,300},{F5,150},{G5,150},

  {E5,150},{C5,150},{D5,150},{B4,450}
};

void playNote(int note, int duration)
{
  if (note == REST)
    noTone(buzzer);
  else
    tone(buzzer, note);

  delay(duration);

  noTone(buzzer);
  delay(duration / 5);
}

void setup()
{
  Nanit_Base_Start();
  pinMode(buzzer, OUTPUT);
}

void loop()
{
  int count = sizeof(mario) / sizeof(mario[0]);

  for (int i = 0; i < count; i++)
  {
    playNote(mario[i].freq, mario[i].dur);
  }
}