#include "Arduino.h"
#include "SevenSegment.h"

SevenSegment::SevenSegment(byte latchPin, byte dataPin, byte clockPin) : m_latchPin(latchPin), m_dataPin(dataPin),  m_clockPin(clockPin) {
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
}

void SevenSegment::showNumber(int number, bool showHex) {
  int disp = number % 100;
  int tensDigit = (abs(disp) / 10) % 10;
  int onesDigit = disp % 10;

  if(showHex) {
    disp = number & 0xFF;
    tensDigit = (number >> 4) & 0xF;
    onesDigit = number & 0xF;
  }

  writeData(numbers[onesDigit], numbers[tensDigit]);

  // Serial.print("number=");
  // Serial.println(number);
  // Serial.print("hex=");
  // Serial.println(disp, HEX);
  // Serial.print("tensDigit=");
  // Serial.println(tensDigit);
  // Serial.print("onesDigit=");
  // Serial.println(onesDigit);
  // Serial.println("*****");
}

void SevenSegment::writeData(byte data1, byte data2) {
  // Serial.print("data1=");
  // Serial.println(data1);
  // Serial.print("data2=");
  // Serial.println(data2);
  // Serial.print("m_latchPin=");
  // Serial.println(m_latchPin);
  // Serial.print("m_dataPin=");
  // Serial.println(m_dataPin);
  // Serial.print("m_clockPin=");
  // Serial.println(m_clockPin);
  // Serial.println("*****");

  digitalWrite(m_latchPin, LOW);

  shiftOut(m_dataPin, m_clockPin, LSBFIRST, data2);
  shiftOut(m_dataPin, m_clockPin, LSBFIRST, data1);
  
  digitalWrite(m_latchPin, HIGH);
}