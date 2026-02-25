#include "SevenSegment.h"
#include <FastLED.h>

// SevenSegment
const byte LATCH_PIN = 12;  // Pin connected to Pin 5 of 74HC595 (Latch)
const byte DATA_PIN = 14;   // Pin connected to Pin 6 of 74HC595 (Data)
const byte CLOCK_PIN = 13;  // Pin connected to Pin 7 of 74HC595 (Clock)

SevenSegment sevseg = SevenSegment(LATCH_PIN, DATA_PIN, CLOCK_PIN);
// end SevenSegment

// Timing
long previousMillis = 0;
long interval = 1000;
// end Timing

// Configuration
#define LED_PIN 15        // Pin connected to data line
#define NUM_LEDS 256      // Number of LEDs in your strip
#define BRIGHTNESS 64     // 0-255 (Keep low for testing)
#define LED_TYPE WS2812B  // Chipset
#define COLOR_ORDER GRB   // Typical color order for WS2812B
#define ANALOG A0

// Array to hold LED color data
CRGB leds[NUM_LEDS];

int val;
int numLedsToLight;

int cols = 32;
int rows = 8;
std::vector<int> getColumnArray(int col, int height);
int colToLight = 0;

void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  Serial.begin(9600);
}

void loop() {
  // val = analogRead(ANALOG);
  // numLedsToLight = map(val, 0, 1023, 0, NUM_LEDS);

  // uint8_t colorShift = beatsin8(10, 0, 255); // 10 bpm speed
  // uint8_t hue = map(colorShift, 0, 255, HUE_RED, HUE_BLUE); // Shift Red to Blue

  // // Pulse Brightness (Value)
  // uint8_t brightness = beatsin8(20, 50, 255); // 20 bpm, min 50, max 255

  // FastLED.clear();
  // fill_solid(leds, numLedsToLight, CHSV(hue, 255, brightness));
  // FastLED.show();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval) {

    std::vector<int> lights = getColumnArray(colToLight, 3);

    FastLED.clear();
    for (int i = 0; i < lights.size(); i++) {
      leds[lights[i]] = CRGB::Blue;
    }
    FastLED.show();

    sevseg.showNumber(colToLight);

    colToLight++;
    if (colToLight >= 32) {
      colToLight = 0;
    }

    previousMillis = currentMillis;
  }
}

// return array of leds to light for a column
std::vector<int> getColumnArray(int col, int height) {
  std::vector<int> lights;
  int colStart;

  if (col % 2 == 1) {
    colStart = col * 8;

    for (int i = colStart; i < (colStart + height); i++) {
      lights.push_back(i);
    }
  } else {
    colStart = col * 8 + 7;

    for (int i = colStart; i > (colStart - height); i--) {
      lights.push_back(i);
    }
  }

  return lights;
}

void printVector(const std::vector<int>& vec) {
  Serial.print("{ ");
  for (const auto& element : vec) {
    Serial.print(element);
    Serial.print(" ");
  }
  Serial.println("}");
}
