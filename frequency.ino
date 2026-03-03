#include <FastLED.h>
#include <driver/i2s_std.h>
#include <arduinoFFT.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789

/**
*   Buttons
*/
#define BUTTON_0  17
#define BUTTON_1  46
#define BUTTON_2  18
struct buttonStates {
  bool state;
  bool stateOld;
};

buttonStates buttons[3] = {0};
unsigned long prevButtonMillis = 0;
unsigned long buttonInterval = 50;

bool crispPeak = true;
bool instant = true;

/**
*   TFT Display Configuration
*/
#define TFT_DC    9
#define TFT_RST   10
#define TFT_CS    11
#define TFT_MOSI  12
#define TFT_SCLK  13
// Initialize with Software SPI
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);


/**
*   Timing
*/
int peakHangTime = 1024;  // the number of millis that it hangs at peak height before descending 
long peakInterval = 64; // the number of millis in between height decrements after hanging

/**
*   LED Matrix Configuration
*/
#define LED_PIN     4         // Pin connected to data line
#define NUM_LEDS    256       // Number of LEDs in your strip
#define BRIGHTNESS  6         // 0-255 (Keep low for testing)
#define LED_TYPE    WS2812B   // Chipset
#define COLOR_ORDER GRB       // Typical color order for WS2812B

CRGB leds[NUM_LEDS];
int fadeTime[13] = {4, 5, 10, 25, 50, 75, 100, 125, 150, 175, 200, 225, 255};
int fadeAmplitudeStyle = 0;
int fadePeakStyle = 3;

struct peakData {
  int height;
  long timestamp;
  int id;
};

struct waveColors {
  CRGB amplitudeColor;
  CRGB peakColor;
};

const int numCols = 32;
const int numRows = 8;
const int SINE_WAVE = 0;
const int LOG_WAVE = 1;
int waveType = LOG_WAVE;

std::vector<int> peakIDs;
std::vector<int> amplitudeIDs;
peakData peaks[numCols] = {0};
int waveHeights[numCols] = {0};
int colHeights[numCols] = {0};

waveColors colors[7] = {
  {CRGB::CornflowerBlue, CRGB::DarkOrange},
  {CRGB::CadetBlue, CRGB::DarkOrange},
  {CRGB::DarkOrange, CRGB::GreenYellow},
  {CRGB::PapayaWhip, CRGB::Plaid},
  {CRGB::GreenYellow, CRGB::Lime},
  {CRGB::Peru, CRGB::SkyBlue},
  {CRGB::RoyalBlue, CRGB::PowderBlue}
};
int currentColor = 0;
CRGB amplitudeColor = colors[currentColor].amplitudeColor;
CRGB peakColor = colors[currentColor].peakColor;

std::vector<int> getColumnArray(int col, int height); // function signature 

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

#define SAMPLES 256
#define SAMPLING_FREQ 22050
// #define SAMPLES 512
// #define SAMPLING_FREQ 44100
#define BANDS 32
double peak_hold = 1000000.0; // Initial guess for "loudest" sound: initial was 1000000.0
double peak_hold_arr[BANDS];  // One for each band
double smoothed_display[BANDS];

double vReal[SAMPLES]; // Real part of the input/output data
double vImag[SAMPLES]; // Imaginary part of the input/output data (initialized to 0)

ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

/**
*   Multi Core 
*/
TaskHandle_t audioTask;

/**
* Setup
*/
void setup() {
  // INIT FASTLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  // INIT ONBOARD LED
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);

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

  // INIT MULTI CORE
  xTaskCreatePinnedToCore(
    generateWave, /* Task function. */
    "Audio",      /* name of task. */
    10000,        /* Stack size of task */
    NULL,         /* parameter of the task */
    1,            /* priority of the task */
    &audioTask,   /* Task handle to keep track of created task */
    0             /* pin task to core 0 */
  );              
  delay(500); 
}

/**
* Loop
*/
void loop() {
  renderWave();

  // handleButton(0, digitalRead(BUTTON_0));    // this one keeps accidentally triggering
  handleButton(1, digitalRead(BUTTON_1));
  handleButton(2, digitalRead(BUTTON_2));
}

// handle button presses
void handleButton(int button, bool state) {
  buttons[button].state = state;

  if ((millis() - prevButtonMillis) > buttonInterval) {
    if(buttons[button].state == 1 && buttons[button].stateOld == 0) {
      if(button == 0) {
        neopixelWrite(RGB_BUILTIN, 5, 0, 0);  // indicator so I know the button press was registered

        if(instant == true) {
          instant = false;
        } else {
          instant = true;
        }
      }

      if(button == 1) {
        neopixelWrite(RGB_BUILTIN, 0, 5, 0);  // indicator so I know the button press was registered

        currentColor++;
        if(currentColor >= sizeof(colors) / sizeof(colors[0])) {
          currentColor = 0;
        }
        amplitudeColor = colors[currentColor].amplitudeColor;
        peakColor = colors[currentColor].peakColor;
      }

      if(button == 2) {
        neopixelWrite(RGB_BUILTIN, 0, 0, 5);  // indicator so I know the button press was registered
        if(crispPeak == true) {
          crispPeak = false;
        } else {
          crispPeak = true;
        }
      }
    }
  }

  if(buttons[button].state != buttons[button].stateOld) {
    buttons[button].stateOld = buttons[button].state;
    prevButtonMillis = millis();
  }
}

void generateWave(void * pvParameters) {
  // GENERATE WAVE DATA
  while(1) {  // infinite loop because this function is getting offloaded to the second core where it will run continuously
    // sineWave();
    logWave();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void sineWave() {   // used for testing the led rendering code before my microphone came in
  for(int i = 0; i < numCols; i++) {
    int height = beatsin8(64, 0, 8, i * 96);  // this generates the y value for each i-th column
    waveHeights[i] = height * random8(height) / height; // add in some random variance
  }
}

void logWave() {  // the main audio processing function that is offloaded to the second core
  int32_t raw_samples[SAMPLES];
  size_t bytes_read = 0;
  double band_values[BANDS] = {0};

  if (i2s_channel_read(rx_handle, raw_samples, sizeof(raw_samples), &bytes_read, 1000) == ESP_OK) {
      
      // 1. Data Prep: Center the signal (Remove DC Bias)
      // there is a hum that the DC current puts into the signal
      // this can be removed by subtracting the average of the signals from each sample
      int64_t sum = 0;
      for (int i = 0; i < SAMPLES; i++) sum += raw_samples[i];
      int32_t mean = sum / SAMPLES;

      for (int i = 0; i < SAMPLES; i++) {
          vReal[i] = (double)(raw_samples[i] - mean); // DC Removal
          vImag[i] = 0.0;
      }

      // 2. FFT Execution
      // the FFT object is instantiated with the arrays so that's why they aren't passed in here
      FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.compute(FFT_FORWARD);
      FFT.complexToMagnitude();

      // 3. Logarithmic Mapping to 32 Bands
      // this is where we run into problems
      double bin_step = pow((double)(SAMPLES/2) / 2.0, 1.0 / BANDS);  // bin_step=1.19 for 1024 SAMPLES
      double current_bin = 2.0;
      double frame_max = 0;

      // this math is for 1024 samples
      // 0-1,i=0: current_bin=2.00 and next_bin=2.38 and int(2.0) < int(2.38) evaluates to 2<2 which is false and nothing gets counted this round
      // 1-2,i=1: current_bin=2.38 and next_bin=2.83 and 2!<2 : no counting
      // 2-3,i=2: current_bin=2.83 and next_bin=3.36 and 2<3 : counting
      // 3-4,1=3: current_bin=3.36 and next_bin=3.99 and 3!<3 : no counting
      // 4-5,i=4: current_bin=3.99 and next_bin=4.74 and 3<4 : counting
      // 5-6,i=5: current_bin=4.74 and next_bin=5.64 and 4<5 : counting
      // 6-7,i=6: current_bin=5.64 and next_bin=6.71 and 5<6 : counting
      // 7-8,i=7: cuttent_bin=6.71 and next_bin=7.98 and 6<7 : counting
      // 8-9,i=8: cuttent_bin=7.98 and next_bin=9.49 and 7<9 : counting
      // after i=8 the loop starts putting more than one sample into a bucket and there doesn't need to be any more hand holding

      for (int i = 0; i < BANDS; i++) {
          double next_bin = current_bin * bin_step;
          double mag_avg = 0;
          double mag_max = 0;
          int count = 0;

          int tmp_current_bin = 0;
          int tmp_next_bin = 1;

          if(i==0) {  // making sure the loop doesn't skip anything
            tmp_current_bin = 0;
            tmp_next_bin = 1;
          } else if (i==1) {
            tmp_current_bin = 1;
            tmp_next_bin = 2;
          } else if (i==2) {
            tmp_current_bin = 2;
            tmp_next_bin = 3;
          } else if (i==3) {
            tmp_current_bin = 3;
            tmp_next_bin = 4;
          } else if (i==4) {
            tmp_current_bin = 4;
            tmp_next_bin = 5;
          } else if (i==5) {
            tmp_current_bin = 5;
            tmp_next_bin = 6;
          } else if (i==6) {
            tmp_current_bin = 6;
            tmp_next_bin = 7;
          } else if (i==7) {
            tmp_current_bin = 7;
            tmp_next_bin = 8;
          } else if (i==8) {
            tmp_current_bin = 8;
            tmp_next_bin = 9;
          } else {
            tmp_current_bin = current_bin;
            tmp_next_bin = next_bin;
          }

          for (int j = (int)tmp_current_bin; j < (int)tmp_next_bin && j < (SAMPLES/2); j++) {
              mag_avg += vReal[j];
              if(vReal[j]>mag_max) mag_max = vReal[j];
              count++;
          }
          if (count > 0) mag_avg /= count;

          mag_avg = mag_max;  // set to max instead of average to make it more active
          
          // Track max for Auto-Gain
          if (mag_avg > frame_max) frame_max = mag_avg;

          // 4. Scaling (The Fix for the "255" problem)
          // Use a higher divisor or dynamic scaling
          int display_val = (int)((mag_avg / peak_hold) * 255.0);
          
          // Constrain
          if (display_val > 255) display_val = 255;
          if (display_val < 0) display_val = 0;

          // Serial.print(display_val);
          // Serial.print(i == BANDS - 1 ? "" : " ");
          current_bin = next_bin;

          waveHeights[i] = map(display_val, 0, 255, 0, 7);  // this is the data that I needed
      }

      // 5. Auto-Gain: Adjust peak_hold based on environment
      if (frame_max > peak_hold) peak_hold = frame_max; // React to loud noise instantly
      else peak_hold = (peak_hold * 0.98) + (frame_max * 0.02); // Slowly drift down
      
      // Prevent peak_hold from becoming too small (noise floor)
      if (peak_hold < 500000.0) peak_hold = 500000.0; 
  }
}

void renderWave() {
  // DRAW COLUMNS
  unsigned long currentMillis = millis();
  amplitudeIDs.clear();

  // calculate peak height decay
  for(int i = 0; i < numCols; i++) {
    if(long(currentMillis) - peaks[i].timestamp > peakInterval) {
      // peakIDs.push_back(peaks[i].id); // save the id of the LED for fadeToBlackBy() later
      peaks[i].height--;
      if(crispPeak) {
        leds[peaks[i].id] = CRGB::Black;  // this is what currently keeps the fadeToBlackBy() for the amplitude LEDs from affecting the peak LEDs
      }
      peaks[i].id = getLEDFromCoordinate(i, peaks[i].height);
      peaks[i].timestamp = currentMillis;
    }
  }

  // Set the height of each of the 32 columns
  for(int i = 0; i < numCols; i++) {
    if(instant==true) { // draw the columns as the same hieght as the wave instantly
      colHeights[i] = waveHeights[i];
    } else {  // animate the height of the columns towarsds the height of the wave
      if(colHeights[i] > waveHeights[i]) {
        colHeights[i]--;
      }

      if(colHeights[i] < waveHeights[i]) {
        colHeights[i]++;
      }
    }

    std::vector<int> colLEDs = getColumnArray(i, colHeights[i]); // get the id numbers for each led that should be lit in the column

    for(int light : colLEDs) {  // for each of those column leds, set their color
      leds[light] = amplitudeColor;
      // amplitudeIDs.push_back(light); // save the id of the LED for fadeToBlackBy() later
    }
  }

  // set peak values
  for(int i = 0; i < numCols; i++) {
    if(colHeights[i] >= peaks[i].height) {
      // I would like for the peaks to completely disappear if the height has been zero for long enough
      // as it works now the peaks sit at the bottom and never turn off
      peaks[i].height = colHeights[i];
      peaks[i].timestamp = currentMillis + peakHangTime;
      peaks[i].id = getLEDFromCoordinate(i, peaks[i].height);
    }
    
    if(peaks[i].height >= 0) {
      leds[peaks[i].id] = peakColor;
      // peakIDs.push_back(peaks[i].id);
    }
  }

  // Serial.print(amplitudeIDs.size());
  // Serial.print(" : ");
  // Serial.print(peakIDs.size());
  // Serial.println();

  // std::sort(peakIDs.begin(), peakIDs.end());
  // auto it = std::unique(peakIDs.begin(), peakIDs.end());
  // peakIDs.erase(it, peakIDs.end());

  // std::sort(amplitudeIDs.begin(), amplitudeIDs.end());
  // it = std::unique(amplitudeIDs.begin(), amplitudeIDs.end());
  // amplitudeIDs.erase(it, amplitudeIDs.end());

  // removeElements(peakIDs, amplitudeIDs);    // this is the one that makes the fade heavier this is the one that should work
                                            // it should remove anything from peakIDs that is in amplitudeIDs since the aIDs are never above the pIDs
  // removeElements(amplitudeIDs, peakIDs); // this is the one that makes the fade lighter

  // Serial.print(amplitudeIDs.size());
  // Serial.print(" : ");
  // Serial.print(peakIDs.size());
  // Serial.print(" : ");
  // Serial.print(peakIDs.size() + amplitudeIDs.size());
  // Serial.println();

  // Serial.print("A: ");
  // for(int i=0; i<amplitudeIDs.size(); i++) {
  //   // Serial.print("A: ");
  //   // Serial.print(amplitudeIDs[i]);
  //   // Serial.print(" : ");
  //   leds[amplitudeIDs[i]].fadeToBlackBy(fadeTime[fadeAmplitudeStyle]);
  // }
  // Serial.println();

  // Serial.print("P: ");
  // for(int i=0; i<peakIDs.size(); i++) {
  //   // Serial.print("P: ");
  //   // Serial.print(peakIDs[i]);
  //   // Serial.print(" : ");
  //   leds[peakIDs[i]].fadeToBlackBy(fadeTime[fadePeakStyle]);
  // }
  // Serial.println();

  // CRGB aIDs[amplitudeIDs.size()];
  // for(int i = 0; i < amplitudeIDs.size(); i++) {
  //   aIDs[i] = leds[amplitudeIDs[i]];
  // }

  // CRGB pIDs[peakIDs.size()];
  // for(int i = 0; i < peakIDs.size(); i++) {
  //   pIDs[i] = leds[peakIDs[i]];
  // }

  // show the LEDs
  // fadeToBlackBy(leds, NUM_LEDS, fadeTime[fadeAmplitudeStyle]);
  // fadeToBlackBy(leds, NUM_LEDS, fadeTime[fadePeakStyle]);
  // fadeToBlackBy(aIDs, amplitudeIDs.size(), fadeTime[fadeAmplitudeStyle]);
  // fadeToBlackBy(peakLEDs, numCols, fadeTime[fadePeakStyle]);
  // fadeToBlackBy(pIDs, peakIDs.size(), fadeTime[fadePeakStyle]);

  // Serial.println("*****");

  // show the LEDs
  fadeToBlackBy(leds, NUM_LEDS, fadeTime[fadeAmplitudeStyle]);
  FastLED.show();

  // Serial.println("*****");
}

// remove elements from the first array that exist in the second
void removeElements(std::vector<int>& arr1, const std::vector<int>& arr2) {
    int i = 0, j = 0;
    while (i < arr1.size() && j < arr2.size()) {
        if (arr1[i] == arr2[j]) {
            arr1.erase(arr1.begin() + i);
            // Do not increment i, as the next element shifted left
            // j can be incremented if arr2 has unique elements
        } else if (arr1[i] < arr2[j]) {
            i++;
        } else {
            j++;
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
