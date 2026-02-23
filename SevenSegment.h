#ifndef SevenSegment_h
#define SevenSegment_h

#include "Arduino.h" // Essential for Arduino functions like pinMode()

class SevenSegment {
  public:
    SevenSegment(byte latchPin, byte dataPin, byte clockPin);
    void showNumber(int number, bool showHex = false);
  private:
    byte m_latchPin;
    byte m_dataPin;
    byte m_clockPin;
    const byte numbers[16] = {
      B11111100, //0
      B01100000, //1
      B11011010, //2
      B11110010, //3
      B01100110, //4
      B10110110, //5
      B10111110, //6
      B11100000, //7
      B11111110, //8
      B11110110, //9
      B11101110, //A
      B11111111, //B.
      B10011100, //C
      B11111101, //D.
      B10011110, //E
      B10001110, //F
    };
    void writeData(byte data1, byte data2);
};

#endif