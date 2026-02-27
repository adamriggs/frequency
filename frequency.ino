#include <FastLED.h>
#include <driver/i2s_std.h>
#include <arduinoFFT.h>

// Timing
long intervalMultiplier = 5; // helps to slow things down if I want it
long prevWaveMillis = 0;
long waveInterval = 48 * intervalMultiplier;
long colInterval = waveInterval / 8;
long prevColMillis = millis() + (colInterval * 2);
long peakInterval = colInterval * 2;
// end Timing

// Configuration
#define LED_PIN 4        // Pin connected to data line
#define NUM_LEDS 256      // Number of LEDs in your strip
#define BRIGHTNESS 6     // 0-255 (Keep low for testing)
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
int peakHangTime = 8;
// CRGB amplitudeColor = CRGB::CornflowerBlue;
// CRGB peakColor = CRGB::LightSlateGray;
CRGB amplitudeColor = CRGB::Blue;
CRGB peakColor = CRGB::Red;
// CRGB amplitudeColor = CRGB::PowderBlue;
// CRGB peakColor = CRGB::PeachPuff;

std::vector<int> getColumnArray(int col, int height);

// i2s stuff
#define I2S_WS  7
#define I2S_SD  5
#define I2S_SCK 6
#define I2S_PORT I2S_NUM_0

i2s_chan_handle_t rx_handle;

// fft stuff
#define SAMPLES 64            // Must be a power of 2 to get 32 frequency bins
#define SAMPLING_FREQ 16000   // 16kHz sampling rate

double vReal[SAMPLES]; // Real part of the input/output data
double vImag[SAMPLES]; // Imaginary part of the input/output data (initialized to 0)

ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

/**
* Setup
*/
void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  neopixelWrite(RGB_BUILTIN, 0, 0, 5);

  Serial.begin(115200);

  // 1. Configure the I2S channel
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);

  // 2. Configure the Standard Mode (Philips/MSB)
  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLING_FREQ), // 16kHz Sample Rate
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_SCK,
      .ws   = (gpio_num_t)I2S_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)I2S_SD,
    },
  };

  // 3. Initialize and Enable
  i2s_channel_init_std_mode(rx_handle, &std_cfg);
  i2s_channel_enable(rx_handle);
}

/**
* Loop
*/
void loop() {
  unsigned long currentMillis = millis();

  // GENERATE WAVE DATA
  if (currentMillis - prevWaveMillis > waveInterval) {
    // for(int i = 0; i < numCols; i++) {
    //   int height = beatsin8(64, 0, 8, i * 96);  // this generates the y value for each i-th column
    //   waveHeights[i] = height * random8(height) / height; // addin some random variance
    // }

    int32_t raw_buffer[SAMPLES];
    size_t bytes_read = 0;

    // Read I2S data using the new driver
    if (i2s_channel_read(rx_handle, raw_buffer, sizeof(raw_buffer), &bytes_read, 1000) == ESP_OK) {
        int samples_read = bytes_read / sizeof(int32_t);
        for (int i = 0; i < samples_read; i++) {
            vReal[i] = (double)(raw_buffer[i] >> 8); // Align 24-bit data if needed
            vImag[i] = 0.0;
        }

        FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.compute(FFT_FORWARD);
        FFT.complexToMagnitude();

        // Print the 32 bins
        // for (int i = 0; i < (SAMPLES / 2); i++) {
            // Serial.printf("Bin %d: %.2f\n", i, vReal[i]);
        // }
    }
    // Serial.println("*****");

    // Constants for mapping
    const double MAX_RAW_VALUE = 8388607.0; // 24-bit max
    const double NOISE_FLOOR = 500.0;       // Ignore values below this to avoid flickering

    for (int i = 0; i < (SAMPLES / 2); i++) {
        // 1. Normalize the FFT bin
      double binVal = vReal[i] / (SAMPLES / 2);

      // 2. Prevent log(0) errors with a tiny offset
      if (binVal < 1.0) binVal = 1.0;

      // 3. Logarithmic Scale: Result is roughly 0.0 to 6.92
      double logValue = log10(binVal);

      // 4. Map and Constrain to 0-7
      // We multiply by SENSITIVITY to reach '7' before the mic actually clips
      int displayValue = (int)(logValue * SENSITIVITY);
      
      // 5. Clean up noise floor and cap the max
      if (logValue < NOISE_FLOOR) displayValue = 0;
      displayValue = constrain(displayValue, 0, 7);

      // Output for debugging
      Serial.printf("Bin %d: %d\n", i, displayValue);
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
