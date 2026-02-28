#include <FastLED.h>
#include <driver/i2s_std.h>
#include <arduinoFFT.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789

/**
*   TFT Display Configuration
*/
#define TFT_DC 9
#define TFT_RST 10
#define TFT_CS 11
#define TFT_MOSI 12
#define TFT_SCLK 13
// Initialize with Software SPI
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);


/**
*   Timing
*/
long peakInterval = 32;
int peakHangTime = 16;  // this is multiplied by peakInterval

/**
*   LED Configuration
*/
#define LED_PIN 4        // Pin connected to data line
#define NUM_LEDS 256      // Number of LEDs in your strip
#define BRIGHTNESS 6     // 0-255 (Keep low for testing)
#define LED_TYPE WS2812B  // Chipset
#define COLOR_ORDER GRB   // Typical color order for WS2812B
#define ANALOG A0

CRGB leds[NUM_LEDS];

/**
*   LED Display Configuration
*/
struct peakData {
  int value;
  long timestamp;
};

const int numCols = 32;
const int numRows = 8;

peakData peaks[numCols] = {0};
int waveHeights[numCols] = {0};
int colHeights[numCols] = {0};
// CRGB amplitudeColor = CRGB::CornflowerBlue;
// CRGB peakColor = CRGB::LightSlateGray;
// CRGB amplitudeColor = CRGB::Red;
// CRGB peakColor = CRGB::Blue;
CRGB amplitudeColor = CRGB::DarkOrange;
CRGB peakColor = CRGB::GreenYellow;
// CRGB amplitudeColor = CRGB::PowderBlue;
// CRGB peakColor = CRGB::PeachPuff;

std::vector<int> getColumnArray(int col, int height);

/**
*   i2s Configuration
*/
#define I2S_WS  7
#define I2S_SD  5
#define I2S_SCK 6
#define I2S_PORT I2S_NUM_0

i2s_chan_handle_t rx_handle;

/**
*   FFT Configuration
*/
#define SAMPLES 1024            // Must be a power of 2 to get 32 frequency bins
#define SAMPLING_FREQ 44100
#define BANDS 32
double peak_hold = 1000000.0; // Initial guess for "loudest" sound
// double peak_hold[BANDS]; // One for each band
// double smoothed_display[BANDS];

double vReal[SAMPLES]; // Real part of the input/output data
double vImag[SAMPLES]; // Imaginary part of the input/output data (initialized to 0)

ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

/**
* Setup
*/
void setup() {
  // INIT FASTLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  // INIT ONBOARD LED
  neopixelWrite(RGB_BUILTIN, 0, 0, 5);

  // INIT SERIAL COMMUNICATION
  Serial.begin(115200);

  // INIT I2S
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLING_FREQ),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_SCK,
      .ws   = (gpio_num_t)I2S_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)I2S_SD,
    },
  };

  i2s_channel_init_std_mode(rx_handle, &std_cfg);
  i2s_channel_enable(rx_handle);

  // INIT TFT
  // tft.initR(INITR_MINI160x80);
  tft.init(80, 160);
  tft.setRotation(3);
  // tft.invertDisplay(true);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  // tft.setTextSize(2);
  tft.println("Hello World!");
  // tft.drawPixel(0, 0, 0xff9900);
}

/**
* Loop
*/
void loop() {
  unsigned long currentMillis = millis();

  // GENERATE WAVE DATA
  // for(int i = 0; i < numCols; i++) {
  //   int height = beatsin8(64, 0, 8, i * 96);  // this generates the y value for each i-th column
  //   waveHeights[i] = height * random8(height) / height; // addin some random variance
  // }

  int32_t raw_samples[SAMPLES];
  size_t bytes_read = 0;
  double band_values[BANDS] = {0};

  if (i2s_channel_read(rx_handle, raw_samples, sizeof(raw_samples), &bytes_read, 1000) == ESP_OK) {
      
      // 1. Data Prep: Center the signal (Remove DC Bias)
      int64_t sum = 0;
      for (int i = 0; i < SAMPLES; i++) sum += raw_samples[i];
      int32_t mean = sum / SAMPLES;

      for (int i = 0; i < SAMPLES; i++) {
          vReal[i] = (double)(raw_samples[i] - mean); // DC Removal
          vImag[i] = 0.0;
      }

      // 2. FFT Execution
      FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.compute(FFT_FORWARD);
      FFT.complexToMagnitude();

      // 3. Logarithmic Mapping to 32 Bands
      double bin_step = pow((double)(SAMPLES/2) / 2.0, 1.0 / BANDS);
      double current_bin = 2.0;
      double frame_max = 0;

      for (int i = 0; i < BANDS; i++) {
          double next_bin = current_bin * bin_step;
          double mag_avg = 0;
          int count = 0;

          for (int j = (int)current_bin; j < (int)next_bin && j < (SAMPLES/2); j++) {
              mag_avg += vReal[j];
              count++;
          }
          if (count > 0) mag_avg /= count;
          
          // Track max for Auto-Gain
          if (mag_avg > frame_max) frame_max = mag_avg;

          // 4. Scaling (The Fix for the "255" problem)
          // Use a higher divisor or dynamic scaling
          int display_val = (int)((mag_avg / peak_hold) * 255.0);
          
          // Constrain
          if (display_val > 255) display_val = 255;
          if (display_val < 0) display_val = 0;

          Serial.print(display_val);
          Serial.print(i == BANDS - 1 ? "" : " ");
          current_bin = next_bin;

          waveHeights[i] = map(display_val, 0, 255, 0, 7);
      }
      Serial.println();

      // 5. Auto-Gain: Adjust peak_hold based on environment
      if (frame_max > peak_hold) peak_hold = frame_max; // React to loud noise instantly
      else peak_hold = (peak_hold * 0.98) + (frame_max * 0.02); // Slowly drift down
      
      // Prevent peak_hold from becoming too small (noise floor)
      if (peak_hold < 500000.0) peak_hold = 500000.0; 
  }

  // DRAW COLUMNS

  // Move the height of each column one step closer to the value of the wave
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
      // as it works now the peaks sit at the bottom and never turn off
      peaks[i].value = colHeights[i];
      peaks[i].timestamp = currentMillis + (peakInterval * peakHangTime);
    }
    
    if(peaks[i].value >= 0) {
      leds[getLEDFromCoordinate(i, peaks[i].value)] = peakColor;
    }
  }

  // show the LEDs
  fadeToBlackBy(leds, NUM_LEDS, 75);
  FastLED.show();

  // calculate peak decay
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

// returns the ordinal position of the LED according to grid coordinate
// 0, 0 is at the bottom left and not the top right, FYI
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

// return the numbers of the LEDs for a column given a height
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

// output vectors to the Serial port
void printVector(const std::vector<int>& vec) {
  Serial.print("{ ");
  for (const auto& element : vec) {
    Serial.print(element);
    Serial.print(" ");
  }
  Serial.println("}");
}
