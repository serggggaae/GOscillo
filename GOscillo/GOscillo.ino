/*
 * ESP32 Oscilloscope using a 320x240 TFT Version 1.09
 * The max software loop sampling rates are 10ksps with 2 channels and 20ksps with a channel.
 * In the I2S DMA mode, it can be set up to 250ksps.
 * + Pulse Generator
 * + PWM DDS Function Generator (23 waveforms)
 * Copyright (c) 2023, Siliconvalley4066
 */
/*
 * Arduino Oscilloscope using a graphic LCD
 * The max sampling rates are 4.3ksps with 2 channels and 8.6ksps with a channel.
 * Copyright (c) 2009, Noriaki Mitsunaga
 */

//#define NOLCD

#ifndef NOLCD
#include <SPI.h>
#include "TFT_eSPI.h"
TFT_eSPI display = TFT_eSPI();
#endif
const int numReadings = 32;
int readings[numReadings];
int readIndex = 0;
float total = 0;
int proc = 0;
float volt = 0;
#include "driver/adc.h"
//#include "esp_task_wdt.h"

#define BUTTON5DIR
#define EEPROM_START 0
#ifdef EEPROM_START
#include <EEPROM.h>
#endif
#include "arduinoFFT.h"
#define FFT_N 256
double vReal[FFT_N];                                                    // Real part array, actually float type
double vImag[FFT_N];                                                    // Imaginary part array
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_N, 1.0);  // Create FFT object
float freq = 0;
float waveFreq[2];  // frequency (Hz)
float waveDuty[2];  // duty ratio (%)
int dataMin[2];     // buffer minimum value (smallest=0)
int dataMax[2];     //        maximum value (largest=4095)
int dataAve[2];     // 10 x average value (use 10x value to keep accuracy. so, max=40950)
int saveTimer;      // remaining time for saving EEPROM
int timeExec;       // approx. execution time of current range setting (ms)
extern byte duty;
extern byte p_range;
extern unsigned short count;
extern long ifreq;
extern byte wave_id;

const int LCD_WIDTH = 320;
const int LCD_HEIGHT = 240;
const int LCD_YMAX = 200;
const int SAMPLES = 300;
const int NSAMP = 1024;
const int DISPLNG = 300;
const int DOTS_DIV = 25;
const int XOFF = 10;
const int YOFF = 20;
const int ad_ch0 = ADC1_CHANNEL_6;  // Analog 34 pin for channel 0 ADC1_CHANNEL_6
const int ad_ch1 = ADC1_CHANNEL_7;  // Analog 35 pin for channel 1 ADC1_CHANNEL_7
const int volts = ADC1_CHANNEL_0;
const long VREF[] = { 81, 83, 170, 425, 837, 1705 };  // reference voltage 3.3V ->  82.5 :   1V/div range (40mV/dot)
                                                      //    125      250           -> 165 : 0.5V/div
                                                      //             626           -> 413 : 0.2V/div
                                                      //             1250           -> 825 : 100mV/div
                                                      //             2500           -> 1650 : 50mV/div
//const int MILLIVOL_per_dot[] = {100, 50, 20, 10, 5}; // mV/dot
//const int ac_offset[] PROGMEM = {1792, -128, -1297, -1679, -1860}; // for OLED
const int ac_offset[] PROGMEM = { 2880, 2752, 234, -1183, -1663, -1915 };  // for Web
const int MODE_ON = 0;
const int MODE_INV = 1;
const int MODE_OFF = 2;
const char Modes[3][4] PROGMEM = { "ON", "INV", "OFF" };
const int TRIG_AUTO = 0;
const int TRIG_NORM = 1;
const int TRIG_SCAN = 2;
const int TRIG_ONE = 3;
const char TRIG_Modes[4][5] PROGMEM = { "Auto", "Norm", "Scan", "One " };
const int TRIG_E_UP = 0;
const int TRIG_E_DN = 1;
#define RATE_MIN 0
#define RATE_MAX 19
#define RATE_NUM 20
#define RATE_DMA 6
#define RATE_DUAL 8
#define RATE_ROLL 16
#define RATE_MAG 1
#define ITEM_MAX 28
const char Rates[RATE_NUM][5] PROGMEM = { "10us", "20us", "50us", "100u", "200u", "500u", " 1ms", "1.3m", " 2ms", " 5ms", "10ms", "20ms", "50ms", "100m", "200m", "0.5s", " 1s ", " 2s ", " 5s ", " 10s" };
const unsigned long HREF[] PROGMEM = { 40, 40, 20, 40, 80, 200, 400, 500, 800, 2000, 4000, 8000, 20000, 40000, 80000, 200000, 400000, 800000, 2000000, 4000000 };
#define RANGE_MIN 0
#define RANGE_MAX 5
#define VRF 5
float vavr = 0;
float vmax = 0;
float vmin = 0;
float vavr0 = 0;
float vavr1 = 0;
float vopt0 = 0;
float vopt1 = 0;
float freqopt = 0;
float vn0 = 0;
float vn1 = 0;
float v01 = 0;
float v02 = 0;
float v11 = 0;
float v12 = 0;
int rangea = 141;
int rangeb = 141;
int rangec = 141;
int ranged = 141;
int rangee = 68;
int rangef = 19;
int rangej = 2;
int rangeh = 1;
const char Ranges[6][5] PROGMEM = { " 2V", " 1V", "0.5V", "0.2V", "0.1V", "50mV" };
byte range0 = RANGE_MIN;
byte range1 = RANGE_MIN;
byte ch0_mode = MODE_ON, ch1_mode = MODE_ON, rate = 0, orate, wrate = 0;
byte trig_mode = TRIG_AUTO, trig_lv = 10, trig_edge = TRIG_E_UP, trig_ch = ad_ch0;
bool Start = true;  // Start sampling
byte item = 0;      // Default item
short ch0_off = 0, ch1_off = 400;
byte data[4][SAMPLES];  // keep twice of the number of channels to make it a double buffer
uint16_t cap_buf[NSAMP], cap_buf1[NSAMP];
uint16_t payload[SAMPLES * 2 + 2];
byte odat00, odat01, odat10, odat11;  // old data buffer for erase
byte sample = 0;                      // index for double buffer
bool fft_mode = false, pulse_mode = false, dds_mode = false, fcount_mode = false;
byte info_mode = 3;  // Text information display mode
bool dac_cw_mode = false;
int trigger_ad;
volatile bool wfft, wdds;
#define LED_BUILTIN 22
int brightness = 105;
#define LEFTPIN 12   // LEFT
#define RIGHTPIN 13  // RIGHT
#define UPPIN 14     // UP
#define DOWNPIN 27   // DOWN
#define CH0DCSW 33   // DC/AC switch ch0
#define CH1DCSW 32   // DC/AC switch ch1
//#define I2CSDA    21  // I2C SDA
//#define I2CSCL    22  // I2C SCL
// DAC_CHANNEL_1  is GPIO25
// DAC_CHANNEL_2  is GPIO26
// ADC1_CHANNEL_0 is GPIO36
// ADC1_CHANNEL_1 is GPIO37
// ADC1_CHANNEL_2 is GPIO38
// ADC1_CHANNEL_3 is GPIO39
// ADC1_CHANNEL_4 is GPIO32
// ADC1_CHANNEL_5 is GPIO33
// ADC1_CHANNEL_6 is GPIO34
// ADC1_CHANNEL_7 is GPIO35
#define BGCOLOR TFT_BLACK
#define GRIDCOLOR TFT_DARKGREY
#define CH1COLOR TFT_GREEN
#define CH2COLOR TFT_YELLOW
#define FRMCOLOR TFT_LIGHTGREY
#define TXTCOLOR TFT_WHITE
#define HIGHCOLOR TFT_CYAN
#define OFFCOLOR TFT_DARKGREY
#define REDCOLOR TFT_RED
#define LED_ON HIGH
#define LED_OFF LOW
#define INFO_OFF 0x20
#define INFO_BIG 0x10
#define INFO_VOL2 0x08
#define INFO_VOL1 0x04
#define INFO_FRQ2 0x02
#define INFO_FRQ1 0x01

TaskHandle_t taskHandle;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  analogWrite(LED_BUILTIN, brightness);
  xTaskCreatePinnedToCore(setup1, "WebProcess", 4096, NULL, 1, &taskHandle, PRO_CPU_NUM);  //Core 0でタスク開始
  pinMode(CH0DCSW, INPUT_PULLUP);                                                          // CH1 DC/AC
  pinMode(CH1DCSW, INPUT_PULLUP);                                                          // CH2 DC/AC
  pinMode(UPPIN, INPUT_PULLUP);                                                            // up
  pinMode(DOWNPIN, INPUT_PULLUP);                                                          // down
  pinMode(RIGHTPIN, INPUT_PULLUP);                                                         // right
  pinMode(LEFTPIN, INPUT_PULLUP);                                                          // left
  pinMode(34, ANALOG);                                                                     // Analog 34 pin for channel 0 ADC1_CHANNEL_6
  pinMode(35, ANALOG);
  pinMode(21, INPUT);  // 1/10 attenuator(Off=High-Z, Enable=Output Low)
  pinMode(17, INPUT);
#ifdef NOLCD
  pinMode(LED_BUILTIN, OUTPUT);  // sets the digital pin as output
#else
  display.init();  // initialise the library
  display.setRotation(3);
  uint16_t calData[5] = { 405, 3518, 203, 3641, 1 };
  display.setTouch(calData);
  display.fillScreen(BGCOLOR);
#endif

//  Serial.begin(115200);
//  Serial.printf("CORE1 = %d\n", xPortGetCoreID());
#ifdef EEPROM_START
  EEPROM.begin(32);  // set EEPROM size. Necessary for ESP32
  loadEEPROM();      // read last settings from EEPROM
#else
  set_default();
#endif
  //  set_default();
  wfft = fft_mode;
  wdds = dds_mode;
  //  DrawGrid();
  //  DrawText();
  //  display.display();
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  adc1_config_width(ADC_WIDTH_BIT_12);
  if (pulse_mode)
    pulse_init();  // calibration pulse output
  if (dds_mode)
    dds_setup_init();
  orate = RATE_DMA + 1;  // old rate befor change
  rate_i2s_mode_config();
}

#ifndef NOLCD
#ifdef DOT_GRID
void DrawGrid() {
  int disp_leng;
  disp_leng = DISPLNG;
  for (int x = 0; x <= disp_leng; x += 5) {  // Horizontal Line
    for (int y = 0; y <= LCD_YMAX; y += DOTS_DIV) {
      display.drawPixel(XOFF + x, YOFF + y, GRIDCOLOR);
      //      CheckSW();
    }
  }
  for (int x = 0; x <= disp_leng; x += DOTS_DIV) {  // Vertical Line
    for (int y = 0; y <= LCD_YMAX; y += 5) {
      display.drawPixel(XOFF + x, YOFF + y, GRIDCOLOR);
      //      CheckSW();
    }
  }
}
#else
void DrawGrid() {
  display.drawFastVLine(XOFF, YOFF, LCD_YMAX, FRMCOLOR);            // left vertical line
  display.drawFastVLine(XOFF + SAMPLES, YOFF, LCD_YMAX, FRMCOLOR);  // right vertical line
  display.drawFastHLine(XOFF, YOFF, SAMPLES, FRMCOLOR);             // top horizontal line
  display.drawFastHLine(XOFF, YOFF + LCD_YMAX, SAMPLES, FRMCOLOR);  // bottom horizontal line

  for (int y = 0; y < LCD_YMAX; y += DOTS_DIV) {
    if (y > 0) {
      display.drawFastHLine(XOFF + 1, YOFF + y, SAMPLES - 1, GRIDCOLOR);  // Draw 9 horizontal lines
    }
    for (int i = 5; i < DOTS_DIV; i += 5) {
      display.drawFastHLine(XOFF + SAMPLES / 2 - 3, YOFF + y + i, 7, GRIDCOLOR);  // Draw the vertical center line ticks
    }
  }
  for (int x = 0; x < SAMPLES; x += DOTS_DIV) {
    if (x > 0) {
      display.drawFastVLine(XOFF + x, YOFF + 1, LCD_YMAX - 1, GRIDCOLOR);  // Draw 11 vertical lines
    }
    for (int i = 5; i < DOTS_DIV; i += 5) {
      display.drawFastVLine(XOFF + x + i, YOFF + LCD_YMAX / 2 - 3, 7, GRIDCOLOR);  // Draw the horizontal center line ticks
    }
  }
}
#endif
#endif

void DrawText() {
#ifndef NOLCD
  if (info_mode & INFO_OFF)
    return;
  if (info_mode & INFO_BIG) {
    display.setTextSize(2);  // Big
  } else {
    display.setTextSize(1);  // Small
  }
#endif

  //  if (info_mode && Start) {
  if (info_mode & (INFO_FRQ1 | INFO_VOL1)) {
    dataAnalize(0);
    if (info_mode & INFO_FRQ1)
      measure_frequency(0);
    if (info_mode & INFO_VOL1)
      measure_voltage(0);
  }
  if (info_mode & (INFO_FRQ2 | INFO_VOL2)) {
    dataAnalize(1);
    if (info_mode & INFO_FRQ2)
      measure_frequency(1);
    if (info_mode & INFO_VOL2)
      measure_voltage(1);
  }
#ifndef NOLCD
  DrawText_big();
  if (!fft_mode)
    draw_trig_level(GRIDCOLOR);  // draw trig_lv mark
#endif
}

#ifndef NOLCD
void draw_trig_level(int color) {  // draw trig_lv mark
  int x, y;

  x = XOFF + DISPLNG + 1;
  y = YOFF + LCD_YMAX - trig_lv - 12;
  display.drawLine(x, y, x + 8, y + 4, color);
  display.drawLine(x + 8, y + 4, x + 8, y - 4, color);
  display.drawLine(x + 8, y - 4, x, y, color);
}

void display_range(byte rng) {
  display.print(Ranges[rng]);
}

void display_rate(void) {
  display.print(Rates[rate]);
}

void display_mode(byte chmode) {
  display.print(Modes[chmode]);
}

void display_trig_mode(void) {
  display.print(TRIG_Modes[trig_mode]);
}

void display_ac(byte pin) {
  if (digitalRead(pin) == LOW) display.print('~');
}

void DrawGrid(int x) {
  if ((x % DOTS_DIV) == 0) {
    for (int y = 0; y <= LCD_YMAX; y += 5)
      display.drawPixel(XOFF + x, YOFF + y, GRIDCOLOR);
  } else if ((x % 5) == 0)
    for (int y = 0; y <= LCD_YMAX; y += DOTS_DIV)
      display.drawPixel(XOFF + x, YOFF + y, GRIDCOLOR);
}

void ClearAndDrawGraph() {
  int clear;
  byte *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
  int disp_leng;
  disp_leng = DISPLNG - 1;
  bool ch1_active = ch1_mode != MODE_OFF && !(rate < RATE_DUAL && ch0_mode != MODE_OFF);
  if (sample == 0)
    clear = 2;
  else
    clear = 0;
  p1 = data[clear + 0];
  p2 = p1 + 1;
  p3 = data[sample + 0];
  p4 = p3 + 1;
  p5 = data[clear + 1];
  p6 = p5 + 1;
  p7 = data[sample + 1];
  p8 = p7 + 1;
#if 0
  for (int x=0; x<disp_leng; x++) {
    display.drawPixel(XOFF+x, YOFF+LCD_YMAX-data[sample+0][x], CH1COLOR);
    display.drawPixel(XOFF+x, YOFF+LCD_YMAX-data[sample+1][x], CH2COLOR);
  }
#else
  for (int x = 0; x < disp_leng; x++) {
    if (ch0_mode != MODE_OFF) {
      display.drawLine(XOFF + x, YOFF + LCD_YMAX - *p1++, XOFF + x + 1, YOFF + LCD_YMAX - *p2++, BGCOLOR);
      display.drawLine(XOFF + x, YOFF + LCD_YMAX - *p3++, XOFF + x + 1, YOFF + LCD_YMAX - *p4++, CH1COLOR);
    }
    if (ch1_active) {
      display.drawLine(XOFF + x, YOFF + LCD_YMAX - *p5++, XOFF + x + 1, YOFF + LCD_YMAX - *p6++, BGCOLOR);
      display.drawLine(XOFF + x, YOFF + LCD_YMAX - *p7++, XOFF + x + 1, YOFF + LCD_YMAX - *p8++, CH2COLOR);
    }
    //    CheckSW();
  }
#endif
}

void ClearAndDrawDot(int i) {
#if 0
  for (int x=0; x<DISPLNG; x++) {
    display.drawPixel(XOFF+i, YOFF+LCD_YMAX-odat01, BGCOLOR);
    display.drawPixel(XOFF+i, YOFF+LCD_YMAX-odat11, BGCOLOR);
    display.drawPixel(XOFF+i, YOFF+LCD_YMAX-data[sample+0][i], CH1COLOR);
    display.drawPixel(XOFF+i, YOFF+LCD_YMAX-data[sample+1][i], CH2COLOR);
  }
#else
  if (i < 1) {
    DrawGrid(i);
    return;
  }
  if (ch0_mode != MODE_OFF) {
    display.drawLine(XOFF + i - 1, YOFF + LCD_YMAX - odat00, XOFF + i, YOFF + LCD_YMAX - odat01, BGCOLOR);
    display.drawLine(XOFF + i - 1, YOFF + LCD_YMAX - data[0][i - 1], XOFF + i, YOFF + LCD_YMAX - data[0][i], CH1COLOR);
  }
  if (ch1_mode != MODE_OFF) {
    display.drawLine(XOFF + i - 1, YOFF + LCD_YMAX - odat10, XOFF + i, YOFF + LCD_YMAX - odat11, BGCOLOR);
    display.drawLine(XOFF + i - 1, YOFF + LCD_YMAX - data[1][i - 1], XOFF + i, YOFF + LCD_YMAX - data[1][i], CH2COLOR);
  }
#endif
  DrawGrid(i);
}
#endif

#define BENDX 3480  // 85% of 4096
#define BENDY 3072  // 75% of 4096

int16_t adc_linearlize(int16_t level) {
  if (level < BENDY) {
    level = map(level, 0, BENDY, 0, BENDX);
  } else {
    level = map(level, BENDY, 4095, BENDX, 4095);
  }
  return level;
}

void scaleDataArray(byte ad_ch, int trig_point) {
  byte *pdata, ch_mode, range;
  short ch_off;
  uint16_t *idata, *qdata, *rdata;
  long a, b;
  int ch;

  if (ad_ch == ad_ch1) {
    ch_off = ch1_off;
    ch_mode = ch1_mode;
    range = range1;
    pdata = data[sample + 1];
    idata = &cap_buf1[trig_point];
    qdata = rdata = payload + SAMPLES;
    ch = 1;
  } else {
    ch_off = ch0_off;
    ch_mode = ch0_mode;
    range = range0;
    pdata = data[sample + 0];
    idata = &cap_buf[trig_point];
    qdata = rdata = payload;
    ch = 0;
  }
  for (int i = 0; i < SAMPLES; i++) {
    *idata = adc_linearlize(*idata);
    a = ((*idata + ch_off) * VREF[range] + 2048) >> 12;
    if (a > LCD_YMAX) a = LCD_YMAX;
    else if (a < 0) a = 0;
    if (ch_mode == MODE_INV)
      a = LCD_YMAX - a;
    *pdata++ = (byte)a;
    b = ((*idata++ + ch_off) * VREF[range] + 101) / 201;
    if (b > 4095) b = 4095;
    else if (b < 0) b = 0;
    if (ch_mode == MODE_INV)
      b = 4095 - b;
    *qdata++ = (int16_t)b;
  }
  if (rate == 0) {
    mag(data[sample + ch], 10);  // x10 magnification for display
  } else if (rate == 1) {
    mag(data[sample + ch], 5);  // x5 magnification for display
  }
  if (rate == 0) {
    mag(rdata, 10);  // x10 magnification for WEB
  } else if (rate == 1) {
    mag(rdata, 5);  // x5 magnification for WEB
  }
}

byte adRead(byte ch, byte mode, int off, int i) {
  int16_t aa = adc1_get_raw((adc1_channel_t)ch);  // ADC read and save approx 46us
  aa = adc_linearlize(aa);
  long a = (((long)aa + off) * VREF[ch == ad_ch0 ? range0 : range1] + 2048) >> 12;
  if (a > LCD_YMAX) a = LCD_YMAX;
  else if (a < 0) a = 0;
  if (mode == MODE_INV)
    a = LCD_YMAX - a;
  long b = (((long)aa + off) * VREF[ch == ad_ch0 ? range0 : range1] + 101) / 201;
  if (b > 4095) b = 4095;
  else if (b < 0) b = 0;
  if (mode == MODE_INV)
    b = 4095 - b;
  if (ch == ad_ch1) {
    cap_buf1[i] = aa;
    payload[i + SAMPLES] = b;
  } else {
    cap_buf[i] = aa;
    payload[i] = b;
  }
  return a;
}

int advalue(int value, long vref, byte mode, int offs) {
  if (mode == MODE_INV)
    value = LCD_YMAX - value;
  return ((long)value << 12) / vref - offs;
}

void set_trigger_ad() {
  if (trig_ch == ad_ch0) {
    trigger_ad = advalue(trig_lv, VREF[range0], ch0_mode, ch0_off);
  } else {
    trigger_ad = advalue(trig_lv, VREF[range1], ch1_mode, ch1_off);
  }
}

void loop() {
  int oad, ad;
  unsigned long auto_time;
  timeExec = 100;
#ifdef NOLCD
  digitalWrite(LED_BUILTIN, LED_ON);  // GPIO2 is used for touch CS
#endif
  if (rate > RATE_DMA) {
    set_trigger_ad();
    auto_time = pow(10, rate / 4) + 5;
    if (trig_mode != TRIG_SCAN) {
      unsigned long st = millis();
      oad = adc1_get_raw((adc1_channel_t)trig_ch) & 0xfffc;
      for (;;) {
        ad = adc1_get_raw((adc1_channel_t)trig_ch) & 0xfffc;

        if (trig_edge == TRIG_E_UP) {
          if (ad > trigger_ad && trigger_ad > oad)
            break;
        } else {
          if (ad < trigger_ad && trigger_ad < oad)
            break;
        }
        oad = ad;

        if (rate > RATE_DMA + 1)
          CheckSW();  // no need for fast sampling
        if (trig_mode == TRIG_SCAN)
          break;
        if (trig_mode == TRIG_AUTO && (millis() - st) > auto_time)
          break;
        if (rate == RATE_DMA + 1) {
          if (trig_mode == TRIG_NORM && (millis() - st) > auto_time)
            break;
          if (trig_mode == TRIG_ONE && (millis() - st) > auto_time)
            break;
        }
      }
    }
  }

  // sample and draw depending on the sampling rate
  if (rate < RATE_ROLL && Start) {
    // change the index for the double buffer
    if (sample == 0)
      sample = 2;
    else
      sample = 0;

    if (rate <= RATE_DMA) {  // channel 0 only I2S DMA sampling (Max 500ksps)
      sample_i2s();
    } else if (rate == RATE_DMA + 1) {  // channel 0 only 50us sampling
      sample_200us(50);
    } else if (rate >= RATE_DMA + 2 && rate <= RATE_DMA + 3) {  // dual channel 100us, 200us sampling
      sample_dual_us(HREF[rate] / 10);
    } else {  // dual channel .5ms, 1ms, 2ms, 5ms, 10ms, 20ms sampling
      sample_dual_ms(HREF[rate] / 10);
    }
#ifdef NOLCD
    digitalWrite(LED_BUILTIN, LED_OFF);  // GPIO2 is used for touch CS
#endif
    draw_screen();
  } else if (Start) {  // 40ms - 400ms sampling
    timeExec = 5000;
    static const unsigned long r_[] PROGMEM = { 40000, 80000, 200000, 400000 };
    unsigned long r;
    int disp_leng;
    disp_leng = DISPLNG;
    //    unsigned long st0 = millis();
    unsigned long st = micros();
    for (int i = 0; i < disp_leng; i++) {
      r = r_[rate - RATE_ROLL];  // rate may be changed in loop
      while ((st - micros()) < r) {
        CheckSW();
        if (rate < RATE_ROLL)
          break;
      }
      if (rate < RATE_ROLL) {  // sampling rate has been changed
#ifndef NOLCD
        display.fillScreen(BGCOLOR);
#endif
        break;
      }
      st += r;
      if (st - micros() > r)
        st = micros();  // sampling rate has been changed to shorter interval
      if (!Start) {
        i--;
        continue;
      }
      odat00 = odat01;      // save next previous data ch0
      odat10 = odat11;      // save next previous data ch1
      odat01 = data[0][i];  // save previous data ch0
      odat11 = data[1][i];  // save previous data ch1
      if (ch0_mode != MODE_OFF) data[0][i] = adRead(ad_ch0, ch0_mode, ch0_off, i);
      if (ch1_mode != MODE_OFF) data[1][i] = adRead(ad_ch1, ch1_mode, ch1_off, i);
      if (ch0_mode == MODE_OFF) payload[0] = -1;
      if (ch1_mode == MODE_OFF) payload[SAMPLES] = -1;
      xTaskNotify(taskHandle, 0, eNoAction);  // notify Websocket server task
#ifndef NOLCD
      ClearAndDrawDot(i);
#endif
    }
#ifndef NOLCD
    DrawGrid(disp_leng);  // right side grid
#endif
    // Serial.println(millis()-st0);
#ifdef NOLCD
    digitalWrite(LED_BUILTIN, LED_OFF);  // GPIO2 is used for touch CS
#endif
    //    DrawGrid();
    DrawText();
  } else {
    DrawText();
  }
  if (trig_mode == TRIG_ONE)
    Start = false;
  CheckSW();
#ifdef EEPROM_START
  saveEEPROM();  // save settings to EEPROM if necessary
#endif
  if (wdds != dds_mode) {
    if (wdds) {
      dds_setup();
    } else {
      dds_close();
    }
    dds_mode = wdds;
  }
}

void draw_screen() {
  //  display.fillScreen(BGCOLOR);
  if (wfft != fft_mode) {
    fft_mode = wfft;
#ifndef NOLCD
    display.fillScreen(BGCOLOR);
#endif
  }
  if (fft_mode) {
    DrawText();
    plotFFT();
  } else {
#ifndef NOLCD
    DrawGrid();
    ClearAndDrawGraph();
#endif
    DrawText();
    if (ch0_mode == MODE_OFF) payload[0] = -1;
    if (ch1_mode == MODE_OFF) payload[SAMPLES] = -1;
  }
  xTaskNotify(taskHandle, 0, eNoAction);  // notify Websocket server task
  delay(10);                              // wait Web task to send it (adhoc fix)
  //  display.display();
}

#define textINFO 214
#define textINFO2 19
void measure_frequency(int ch) {
  int x1, x2;
  byte y;
  freqDuty(ch);
#ifndef NOLCD
  if (info_mode & INFO_BIG) {
    x1 = textINFO, x2 = x1 + 24;  // Big
  } else {
    x1 = textINFO + 48, x2 = x1 + 12;  // Small
  }
  if (ch == 0) {
    y = 22;
    display.setTextColor(CH1COLOR, BGCOLOR);
  } else {
    if (info_mode & INFO_BIG)
      y = 122;
    else y = 172;
    display.setTextColor(CH2COLOR, BGCOLOR);
  }
  TextBG(&y, x1, 8);
  float freq = waveFreq[ch];
  if (freq < 9.8)
    display.print(freq, 4);
  else if (freq < 99.8)
    display.print(freq, 3);
  else if (freq < 998.0)
    display.print(freq, 2);
  else if (freq < 9998.0) {
    display.print(freq / 1000, 3);
    display.print('k');
  } else if (freq < 99998.0) {
    display.print(freq / 1000, 2);
    display.print('k');
  } else if (freq < 999998.0) {
    display.print(freq / 1000, 1);
    display.print('k');
  } else if (freq < 9999998.0) {
    display.print(freq / 1000000, 3);
    display.print('m');
  } else {
    display.print(freq / 1000000, 2);
    display.print('m');
  }
  display.print("Hz");
  if (fft_mode) return;
  TextBG(&y, x2, 6);
  display.print(waveDuty[ch], 1);
  display.print('%');
#endif
}

void measure_voltage(int ch) {
  int x;
  int xx;
  byte y;
  byte yy;
  if (fft_mode) return;
  if (info_mode & INFO_BIG) {
    x = textINFO, y = 62;  // Big
    xx = textINFO2, yy = 26;
  } else {
    x = textINFO + 48, y = 42;  // Small
    xx = textINFO2, yy = 30;
  }
  if (digitalRead(CH0DCSW) == LOW) {
    if (ch == 0) {
      display.setTextColor(CH1COLOR, BGCOLOR);
      if (range0 == 0) {        // if 1/10 attenuator required
        pinMode(17, OUTPUT);    // assign attenuator controle pin to OUTPUT,
        digitalWrite(17, LOW);  // and output LOW (output 0V)
        vavr = VRF * dataAve[ch] / 40950.0 * 1.295 - v01;
        vmax = VRF * dataMax[ch] / 4095.0 * 1.295 - v01;
        vmin = VRF * dataMin[ch] / 4095.0 * 1.295 - v01;
        vavr0 = vavr;
        vopt0 = vmax - vmin;
      } else {
        pinMode(17, INPUT);
        vavr = VRF * dataAve[ch] / 40950.0 * 0.672 - v02;
        vmax = VRF * dataMax[ch] / 4095.0 * 0.672 - v02;
        vmin = VRF * dataMin[ch] / 4095.0 * 0.672 - v02;
        vavr0 = vavr;
        vopt0 = vmax - vmin;
      }
    }
  } else {
    if (ch == 0) {
      display.setTextColor(CH1COLOR, BGCOLOR);
      if (range0 == 0) {        // if 1/10 attenuator required
        pinMode(17, OUTPUT);    // assign attenuator controle pin to OUTPUT,
        digitalWrite(17, LOW);  // and output LOW (output 0V)
        vavr = VRF * dataAve[ch] / 40950.0 * 1.295;
        vmax = VRF * dataMax[ch] / 4095.0 * 1.295;
        vmin = VRF * dataMin[ch] / 4095.0 * 1.295;
        vopt0 = vmax;
      } else {
        pinMode(17, INPUT);
        vavr = VRF * dataAve[ch] / 40950.0 * 0.672;
        vmax = VRF * dataMax[ch] / 4095.0 * 0.672;
        vmin = VRF * dataMin[ch] / 4095.0 * 0.672;
        vopt0 = vmax;
      }
    }
  }
  if (digitalRead(CH1DCSW) == LOW) {
    if (ch == 1) {
      if (info_mode & INFO_BIG)
        y += 100;
      else y += 150;
      yy += 175;
      display.setTextColor(CH2COLOR, BGCOLOR);
      if (range1 == 0) {        // if 1/10 attenuator required
        pinMode(21, OUTPUT);    // assign attenuator controle pin to OUTPUT,
        digitalWrite(21, LOW);  // and output LOW (output 0V)
        vavr = VRF * dataAve[ch] / 40950.0 * 1.304 - v11;
        vmax = VRF * dataMax[ch] / 4095.0 * 1.304 - v11;
        vmin = VRF * dataMin[ch] / 4095.0 * 1.304 - v11;
        vavr1 = vavr;
        vopt1 = vmax - vmin;
      } else {
        pinMode(21, INPUT);
        vavr = VRF * dataAve[ch] / 40950.0 * 0.672 - v12;
        vmax = VRF * dataMax[ch] / 4095.0 * 0.672 - v12;
        vmin = VRF * dataMin[ch] / 4095.0 * 0.672 - v12;
        vavr1 = vavr;
        vopt1 = vmax - vmin;
      }
    }
  } else {
    if (ch == 1) {
      if (info_mode & INFO_BIG)
        y += 100;
      else y += 150;
      yy += 175;
      display.setTextColor(CH2COLOR, BGCOLOR);
      if (range1 == 0) {        // if 1/10 attenuator required
        pinMode(21, OUTPUT);    // assign attenuator controle pin to OUTPUT,
        digitalWrite(21, LOW);  // and output LOW (output 0V)
        vavr = VRF * dataAve[ch] / 40950.0 * 1.304;
        vmax = VRF * dataMax[ch] / 4095.0 * 1.304;
        vmin = VRF * dataMin[ch] / 4095.0 * 1.304;
        vopt1 = vmax;
      } else {
        pinMode(21, INPUT);
        vavr = VRF * dataAve[ch] / 40950.0 * 0.672;
        vmax = VRF * dataMax[ch] / 4095.0 * 0.672;
        vmin = VRF * dataMin[ch] / 4095.0 * 0.672;
        vopt1 = vmax;
      }
    }
  }
#ifndef NOLCD
  TextBG(&yy, xx, 8);
  if (ch == 0) {
    if (digitalRead(CH0DCSW) == LOW)
      display.print("AC ");
    else
      display.print("DC ");
  }
  if (ch == 1) {
    if (digitalRead(CH1DCSW) == LOW)
      display.print("AC ");
    else
      display.print("DC ");
  }
  display.print(vmax - vmin);
  display.print('V');
  TextBG(&y, x, 8);
  display.print("max");
  display.print(vmax);
  if (vmax >= 0.0) display.print('V');
  TextBG(&y, x, 8);
  display.print("avr");
  display.print(vavr);
  if (vavr >= 0.0) display.print('V');
  TextBG(&y, x, 8);
  display.print("min");
  display.print(vmin);
  if (vmin >= 0.0) display.print('V');
#endif
}
void sample_dual_us(unsigned int r) {  // dual channel. r > 67
  if (ch0_mode != MODE_OFF && ch1_mode == MODE_OFF) {
    unsigned long st = micros();
    for (int i = 0; i < SAMPLES; i++) {
      while (micros() - st < r)
        ;
      cap_buf[i] = adc1_get_raw((adc1_channel_t)ad_ch0);
      st += r;
    }
    scaleDataArray(ad_ch0, 0);
    memset(data[1], 0, SAMPLES);
  } else if (ch0_mode == MODE_OFF && ch1_mode != MODE_OFF) {
    unsigned long st = micros();
    for (int i = 0; i < SAMPLES; i++) {
      while (micros() - st < r)
        ;
      cap_buf1[i] = adc1_get_raw((adc1_channel_t)ad_ch1);
      st += r;
    }
    scaleDataArray(ad_ch1, 0);
    memset(data[0], 0, SAMPLES);
  } else {
    unsigned long st = micros();
    for (int i = 0; i < SAMPLES; i++) {
      while (micros() - st < r)
        ;
      cap_buf[i] = adc1_get_raw((adc1_channel_t)ad_ch0);
      cap_buf1[i] = adc1_get_raw((adc1_channel_t)ad_ch1);
      st += r;
    }
    scaleDataArray(ad_ch0, 0);
    scaleDataArray(ad_ch1, 0);
  }
}

void sample_dual_ms(unsigned int r) {  // dual channel. r > 500
                                       // .5ms, 1ms or 2ms sampling
  unsigned long st = micros();
  for (int i = 0; i < SAMPLES; i++) {
    while (micros() - st < r)
      ;
    st += r;
    if (ch0_mode != MODE_OFF) {
      cap_buf[i] = adc1_get_raw((adc1_channel_t)ad_ch0);
    }
    if (ch1_mode != MODE_OFF) {
      cap_buf1[i] = adc1_get_raw((adc1_channel_t)ad_ch1);
    }
  }
  //  if (ch0_mode == MODE_OFF) memset(data[0], 0, SAMPLES);
  //  if (ch1_mode == MODE_OFF) memset(data[1], 0, SAMPLES);
  scaleDataArray(ad_ch0, 0);
  scaleDataArray(ad_ch1, 0);
}

void sample_200us(unsigned int r) {  // adc1_get_raw() with timing, channel 0 or 1. 1250us/div 20ksps
  uint16_t *idata;
  int ad_ch;
  if (ch0_mode == MODE_OFF && ch1_mode != MODE_OFF) {
    ad_ch = ad_ch1;
    idata = cap_buf1;
  } else {
    ad_ch = ad_ch0;
    idata = cap_buf;
  }
  unsigned long st = micros();
  //  disableCore1WDT();
  for (int i = 0; i < SAMPLES; i++) {
    while (micros() - st < r)
      ;
    *idata++ = adc1_get_raw((adc1_channel_t)ad_ch);
    st += r;
    //    yield();
    //    esp_task_wdt_reset();
  }
  //  enableCore1WDT();
  delay(1);
  scaleDataArray(ad_ch, 0);
  delay(1);
}

void plotFFT() {
  byte *lastplot, *newplot;
  int ylim = 200;

  int clear = (sample == 0) ? 2 : 0;
  for (int i = 0; i < FFT_N; i++) {
    vReal[i] = cap_buf[i];
    vImag[i] = 0.0;
  }
  FFT.dcRemoval();
  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);  // Weigh data
  FFT.compute(FFTDirection::Forward);                     // Compute FFT
  FFT.complexToMagnitude();                               // Compute magnitudes
  newplot = data[sample];
  lastplot = data[clear];
  payload[0] = 0;
  for (int i = 1; i < FFT_N / 2; i++) {
    float db = log10(vReal[i]);
    payload[i] = constrain((int)(1024.0 * (db - 1.6)), 0, 4095);
#ifndef NOLCD
    int dat = constrain((int)(50.0 * (db - 1.6)), 0, ylim);
    display.drawFastVLine(i * 2, ylim - lastplot[i], lastplot[i], BGCOLOR);  // erase old
    display.drawFastVLine(i * 2, ylim - dat, dat, CH1COLOR);
    newplot[i] = dat;
#endif
  }
  draw_scale();
}

void draw_scale() {
  int ylim = 204;
  float fhref, nyquist;
#ifndef NOLCD
  display.setTextColor(TXTCOLOR);
  display.setCursor(0, ylim);
  display.print("0Hz");
#endif
  fhref = freqhref();
  nyquist = 5.0e6 / fhref;  // Nyquist frequency
  long inyquist = nyquist;
  payload[FFT_N / 2] = (short)(inyquist / 1000);
  payload[FFT_N / 2 + 1] = (short)(inyquist % 1000);
#ifndef NOLCD
  if (nyquist > 999.0) {
    nyquist = nyquist / 1000.0;
    if (nyquist > 99.5) {
      display.setCursor(116, ylim);
      display.print(nyquist / 2, 0);
      display.print('k');
      display.setCursor(232, ylim);
      display.print(nyquist, 0);
    } else if (nyquist > 9.95) {
      display.setCursor(122, ylim);
      display.print(nyquist / 2, 0);
      display.print('k');
      display.setCursor(238, ylim);
      display.print(nyquist, 0);
    } else {
      display.setCursor(122, ylim);
      display.print(nyquist / 2, 1);
      display.print('k');
      display.setCursor(232, ylim);
      display.print(nyquist, 1);
    }
    display.print('k');
  } else {
    display.setCursor(116, ylim);
    display.print(nyquist / 2, 0);
    display.setCursor(238, ylim);
    display.print(nyquist, 0);
  }
#endif
}

float freqhref() {
  return (float)HREF[rate];
}

#ifdef EEPROM_START
void saveEEPROM() {  // Save the setting value in EEPROM after waiting a while after the button operation.
  int p = EEPROM_START;
  if (saveTimer > 0) {                 // If the timer value is positive
    saveTimer = saveTimer - timeExec;  // Timer subtraction
    if (saveTimer <= 0) {              // if time up
      EEPROM.write(p++, range0);       // save current status to EEPROM
      EEPROM.write(p++, ch0_mode);
      EEPROM.write(p++, lowByte(ch0_off));  // save as Little endian
      EEPROM.write(p++, highByte(ch0_off));
      EEPROM.write(p++, range1);
      EEPROM.write(p++, ch1_mode);
      EEPROM.write(p++, lowByte(ch1_off));  // save as Little endian
      EEPROM.write(p++, highByte(ch1_off));
      EEPROM.write(p++, rate);
      EEPROM.write(p++, trig_mode);
      EEPROM.write(p++, trig_lv);
      EEPROM.write(p++, trig_edge);
      EEPROM.write(p++, trig_ch);
      EEPROM.write(p++, fft_mode);
      EEPROM.write(p++, info_mode);
      EEPROM.write(p++, item);
      EEPROM.write(p++, pulse_mode);
      EEPROM.write(p++, duty);
      EEPROM.write(p++, p_range);
      EEPROM.write(p++, lowByte(count));  // save as Little endian
      EEPROM.write(p++, highByte(count));
      EEPROM.write(p++, dds_mode);
      EEPROM.write(p++, wave_id);
      EEPROM.write(p++, ifreq & 0xff);
      EEPROM.write(p++, (ifreq >> 8) & 0xff);
      EEPROM.write(p++, (ifreq >> 16) & 0xff);
      EEPROM.write(p++, (ifreq >> 24) & 0xff);
      EEPROM.write(p++, dac_cw_mode);
      EEPROM.commit();  // actually write EEPROM. Necessary for ESP32
    }
  }
}
#endif

void set_default() {
  range0 = RANGE_MIN;
  ch0_mode = MODE_ON;
  ch0_off = 0;
  range1 = RANGE_MIN;
  ch1_mode = MODE_ON;
  ch1_off = 2048;
  rate = 6;
  trig_mode = TRIG_AUTO;
  trig_lv = 20;
  trig_edge = TRIG_E_UP;
  trig_ch = ad_ch0;
  fft_mode = false;
  info_mode = 1;  // display frequency and duty.  Voltage display is off
  item = 0;       // menu item
  pulse_mode = true;
  duty = 128;    // PWM 50%
  p_range = 16;  // PWM range
  count = 256;   // PWM 1220Hz
  dds_mode = false;
  wave_id = 0;    // sine wave
  ifreq = 23841;  // 238.41Hz
  dac_cw_mode = false;
}

extern const byte wave_num;

#ifdef EEPROM_START
void loadEEPROM() {  // Read setting values from EEPROM (abnormal values will be corrected to default)
  int p = EEPROM_START, error = 0;

  range0 = EEPROM.read(p++);  // range0
  if ((range0 < RANGE_MIN) || (range0 > RANGE_MAX)) ++error;
  ch0_mode = EEPROM.read(p++);  // ch0_mode
  if (ch0_mode > 2) ++error;
  *((byte *)&ch0_off) = EEPROM.read(p++);      // ch0_off low
  *((byte *)&ch0_off + 1) = EEPROM.read(p++);  // ch0_off high
  if ((ch0_off < -8192) || (ch0_off > 8191)) ++error;

  range1 = EEPROM.read(p++);  // range1
  if ((range1 < RANGE_MIN) || (range1 > RANGE_MAX)) ++error;
  ch1_mode = EEPROM.read(p++);  // ch1_mode
  if (ch1_mode > 2) ++error;
  *((byte *)&ch1_off) = EEPROM.read(p++);      // ch1_off low
  *((byte *)&ch1_off + 1) = EEPROM.read(p++);  // ch1_off high
  if ((ch1_off < -8192) || (ch1_off > 8191)) ++error;

  rate = EEPROM.read(p++);  // rate
  if ((rate < RATE_MIN) || (rate > RATE_MAX)) ++error;
  //  if (ch0_mode == MODE_OFF && rate < 5) ++error;  // correct ch0_mode
  trig_mode = EEPROM.read(p++);  // trig_mode
  if (trig_mode > TRIG_SCAN) ++error;
  trig_lv = EEPROM.read(p++);  // trig_lv
  if (trig_lv > LCD_YMAX) ++error;
  trig_edge = EEPROM.read(p++);  // trig_edge
  if (trig_edge > 1) ++error;
  trig_ch = EEPROM.read(p++);  // trig_ch
  if (trig_ch != ad_ch0 && trig_ch != ad_ch1) ++error;
  fft_mode = EEPROM.read(p++);   // fft_mode
  info_mode = EEPROM.read(p++);  // info_mode
  if (info_mode > 63) ++error;
  item = EEPROM.read(p++);  // item
  if (item > ITEM_MAX) ++error;
  pulse_mode = EEPROM.read(p++);  // pulse_mode
  duty = EEPROM.read(p++);        // duty
  p_range = EEPROM.read(p++);     // p_range
  if (p_range > 16) ++error;
  *((byte *)&count) = EEPROM.read(p++);      // count low
  *((byte *)&count + 1) = EEPROM.read(p++);  // count high
  if (count > 256) ++error;
  dds_mode = EEPROM.read(p++);  // DDS wave id
  wave_id = EEPROM.read(p++);   // DDS wave id
  if (wave_id >= wave_num) ++error;
  *((byte *)&ifreq) = EEPROM.read(p++);      // ifreq low
  *((byte *)&ifreq + 1) = EEPROM.read(p++);  // ifreq
  *((byte *)&ifreq + 2) = EEPROM.read(p++);  // ifreq
  *((byte *)&ifreq + 3) = EEPROM.read(p++);  // ifreq high
  if (ifreq > 99999L) ++error;
  dac_cw_mode = EEPROM.read(p++);  // DDS wave id
  if (error > 0)
    set_default();
}
#endif