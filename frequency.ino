#include "SevenSegment.h"
#include <FastLED.h>

// SevenSegment
const byte LATCH_PIN = 12;  // Pin connected to Pin 5 of 74HC595 (Latch)
const byte DATA_PIN = 14;   // Pin connected to Pin 6 of 74HC595 (Data)
const byte CLOCK_PIN = 13;  // Pin connected to Pin 7 of 74HC595 (Clock)

SevenSegment sevseg = SevenSegment(LATCH_PIN, DATA_PIN, CLOCK_PIN);
// end SevenSegment

// Timing
long intervalMultiplier = 5; // helps to slow things down if I want it
long prevWaveMillis = 0;
long waveInterval = 48 * intervalMultiplier;
long colInterval = waveInterval / 8;
long prevColMillis = millis() + (colInterval * 2);
long peakInterval = colInterval * 2;
// end Timing

// Configuration
#define LED_PIN 15        // Pin connected to data line
#define NUM_LEDS 256      // Number of LEDs in your strip
#define BRIGHTNESS 16     // 0-255 (Keep low for testing)
#define LED_TYPE WS2812B  // Chipset
#define COLOR_ORDER GRB   // Typical color order for WS2812B
#define ANALOG A0

// Array to hold LED color data
CRGB leds[NUM_LEDS];

// display data
struct peakData {
  int value;
  long timestamp;
};

const int numCols = 32;
const int numRows = 8;

peakData peaks[numCols] = {0};
int waveHeights[numCols] = {0};
int colHeights[numCols] = {0};
int peakHangTime = 4;
// CRGB amplitudeColor = CRGB::CornflowerBlue;
// CRGB peakColor = CRGB::LightSlateGray;
CRGB amplitudeColor = CRGB::Blue;
CRGB peakColor = CRGB::Red;

std::vector<int> getColumnArray(int col, int height);

void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  Serial.begin(9600);
}

void loop() {
  unsigned long currentMillis = millis();
  std::vector<int> allCols;

  // GENERATE WAVE DATA
  if (currentMillis - prevWaveMillis > waveInterval) {
    for(int i = 0; i < numCols; i++) {
      int height = beatsin8(64, 0, 8, i * 96);  // this generates the y value for each i-th column
      waveHeights[i] = height * random8(height) / height; // addin some random variance
    }
  }

  // DRAW COLUMNS
  if (currentMillis - prevColMillis > colInterval) {
    FastLED.clear();

    // set the height of the columns of light
    for(int i = 0; i < numCols; i++) {
      if(colHeights[i] > waveHeights[i]){
        colHeights[i]--;
      }

      if(colHeights[i] < waveHeights[i]){
        colHeights[i]++;
      }

      std::vector<int> colLEDs = getColumnArray(i, colHeights[i]); // get the id numbers for each led in the column

      for(int light : colLEDs) {  // for each of those column leds, set their color
        leds[light] = amplitudeColor;
      }
    }

    // set peak values
    for(int i = 0; i < numCols; i++) {
      if(colHeights[i] >= peaks[i].value){
        // I would like for the peaks to completely disappear if the height has been zero for long enough
        peaks[i].value = colHeights[i];
        peaks[i].timestamp = currentMillis + (peakInterval * peakHangTime);
      }
      
      if(peaks[i].value >= 0) {
        leds[getLEDFromCoordinate(i, peaks[i].value)] = peakColor;
      }
    }

    FastLED.show();
    prevColMillis = currentMillis;
  }

  // PEAK DECAY
  for(int i = 0; i < numCols; i++) {
    if(long(currentMillis) - peaks[i].timestamp > peakInterval) {
      peaks[i].value--;
      peaks[i].timestamp = currentMillis;

      if(peaks[i].value < 0){
        peaks[i].value = -1;
      }
    }
  }
}

int getLEDFromCoordinate(int x, int y) {
  int light = 0;
  int xStart;

  if (x % 2 == 1) {
    xStart = x * 8;
    light = xStart + y;
  } else {
    xStart = x * 8 + 7;
    light = xStart - y;
  }

  return light;
}

// return array of leds to light for a column
std::vector<int> getColumnArray(int col, int height) {
  std::vector<int> colLEDs;
  int colStart;

  if (col % 2 == 1) {
    colStart = col * 8;

    for (int i = colStart; i < (colStart + height); i++) {
      colLEDs.push_back(i);
    }
  } else {
    colStart = col * 8 + 7;

    for (int i = colStart; i > (colStart - height); i--) {
      colLEDs.push_back(i);
    }
  }

  return colLEDs;
}

void printVector(const std::vector<int>& vec) {
  Serial.print("{ ");
  for (const auto& element : vec) {
    Serial.print(element);
    Serial.print(" ");
  }
  Serial.println("}");
}
