#include "SevenSegment.h"
#include <FastLED.h>

// SevenSegment
const byte LATCH_PIN = 12;  // Pin connected to Pin 5 of 74HC595 (Latch)
const byte DATA_PIN = 14;   // Pin connected to Pin 6 of 74HC595 (Data)
const byte CLOCK_PIN = 13;  // Pin connected to Pin 7 of 74HC595 (Clock)

SevenSegment sevseg = SevenSegment(LATCH_PIN, DATA_PIN, CLOCK_PIN);
// end SevenSegment

// Timing
long intervalMultiplier = 1; // helps to slow things down if I want it
long prevFreqMillis = 0;
long freqInterval = 50 * intervalMultiplier;
long peakInterval = 75 * intervalMultiplier;
// end Timing

// Configuration
#define LED_PIN 15        // Pin connected to data line
#define NUM_LEDS 256      // Number of LEDs in your strip
#define BRIGHTNESS 4      // 0-255 (Keep low for testing)
#define LED_TYPE WS2812B  // Chipset
#define COLOR_ORDER GRB   // Typical color order for WS2812B
#define ANALOG A0

// Array to hold LED color data
CRGB leds[NUM_LEDS];

// 8x32 code
int cols = 32;
int rows = 8;
std::vector<int> getColumnArray(int col, int height);
int colToLight = 0;
int heights[32] = {1};
int peakHangTime = 2;
CRGB amplitudeColor = CRGB::CornflowerBlue;
CRGB peakColor = CRGB::LightSlateGray;

struct peakData {
  int value;
  long timestamp;
};

peakData peaks[32] = {0};

void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  Serial.begin(9600);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - prevFreqMillis > freqInterval) {  // don't update every loop because that makes things blurry
    FastLED.clear();

    for(int i = 0; i < cols; i++) {
      int height = beatsin8(64, 0, 8, i * 96);  // this generates the y value for each x as the i-th column. I was trying to use multipels of 32 to keep the sine wave from appearing to move to the right
      heights[i] = height * random8(height) / height; // addin some random variance
      std::vector<int> colLEDs = getColumnArray(i, heights[i]); // get the id numbers for the column of leds

      for(int light : colLEDs) {  // for each of those column leds, set their color
        leds[light] = amplitudeColor;
      }
    }

    for(int i = 0; i < cols; i++) { // this finds the peak values for each column using the height that was calculated in the last for loop
      if(heights[i] >= peaks[i].value) {
        peaks[i].value = heights[i];
        peaks[i].timestamp = currentMillis + (peakInterval * peakHangTime);
      }
      
      if(peaks[i].value > -1) {
        leds[getLEDFromCoordinate(i, peaks[i].value)] = peakColor;
      }
      // leds[getLEDFromCoordinate(i, peaks[i].value)] = peakColor;
    }

    FastLED.show();
    prevFreqMillis = currentMillis;
  }

  for(int i = 0; i < cols; i++) {
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
    light = xStart + y - 1;
  } else {
    xStart = x * 8 + 7;
    light = xStart - y + 1;
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
