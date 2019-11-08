/*
  Author: Benjamin Low (Lthben@gmail.com)
  Date: Nov 2019
  Description: Air sculptures 1 and 2 - CO2 and PM25

      This version uses the Arduino Mega 2560. No audio functionality. This is for sculpture 1 and 2 since the 3.3V teensy is unable to drive a long 24V led strip, and a 5V microcontroller and signal is needed for the data line.

      Each sculpture has two buttons and one distance sensor each.
      The two buttons play back the two sets of air measurements readings translated into brightness values. 
      One button represents one set of reading and one strip of led.
      There is an idle mode pulsing light animation. Active mode is triggered by button and will show a sequence of brightness values. 
      The distance sensor changes the hue of the leds in real time. 
      There is a sound for idle mode and one for active playback mode. Sound disabled for the Arduino since it does not have an audio shield.
*/

#include <Arduino.h>
#include <Bounce2.h>
#include <FastLED.h>
#include <elapsedMillis.h>
#include <Adafruit_VL53L0X.h>

//-------------------- USER DEFINED SETTINGS --------------------//

//Uncomment one below
// #define __CO2__
#define __PM25__ 
// #define __VOC__

//PINOUTS for LED strips
const int CO2STRIP1_1PIN = 7, CO2STRIP1_2PIN = 6, CO2STRIP1_3PIN = 5, CO2STRIP2PIN = 4;//for CO2
const int STRIP1PIN = 5, STRIP2PIN = 4;//for PM25 and VOC

//PINOUTS for buttons
const int button0pin = 14, button1pin = 15;

//PINOUTS for dist sensor
//SCL to 21 and SDA to 20

const int CO2band1_1 = 25, CO2band1_2 = 25, CO2band1_3 = 25, CO2band2 = 55, PM25band1 = 55, PM25band2 = 55, VOCband1 = 50, VOCband2 = 50; //num of pixels per strip. Each pixel is 10cm.

const int CO2_1[17] = { 1609, 577, 406, 419, 443, 414, 403, 413, 409, 411, 412, 409, 423, 414, 421, 434, 421 };
const int CO2_2[40] = { 1685, 642, 618, 698, 697, 778, 450, 664, 648, 676, 425, 504, 550, 481, 640, 942, 1791, 504, 733, 688, 592, 608, 850, 779, 1876, 646, 648, 659, 893, 422, 455, 701, 716, 892, 1046, 455, 483, 503, 448, 550 };

const int PM25_1[20] = { 118, 38, 34, 111, 125, 82, 178, 174, 43, 43, 42, 83, 63, 83, 85, 103, 68, 53, 54, 66 };
const int PM25_2[32] = { 65, 88, 44, 42, 73, 69, 70, 61, 54, 89, 86, 91, 60, 63, 92, 88, 95, 55, 85, 49, 48, 51, 35, 38, 49, 51, 21, 32, 28, 42, 21, 25 };

const int VOC_1[26] = { 8, 11, 5, 13, 16, 14, 15, 17, 15, 20, 29, 21, 22, 19, 14, 13, 19, 25, 17, 15, 13, 17, 16, 15, 20, 17 };
const int VOC_2[22] = { 122, 67, 24, 36, 46, 32, 29, 34, 27, 25, 22, 23, 19, 23, 21, 33, 26, 34, 41, 15, 25, 18 };

CHSV cblue(140,255,255);
const int BAND_DELAY = 500;   //controls led animation speed

//-------------------- Buttons and distance sensor --------------------//
Bounce button0 = Bounce(button0pin, 15); // 15 = 15 ms debounce time
Bounce button1 = Bounce(button1pin, 15);

bool isButton0Pressed, isButton1Pressed; //track response to button triggered

Adafruit_VL53L0X lox = Adafruit_VL53L0X(); //SCL to 21 and SDA to 20
int rangeVal; //reading in mm
elapsedMillis loxmsec; //to track that it takes measurement at an interval of around 100ms instead of continuously
bool isUserPresent = false;

//-------------------- Light --------------------//

#define LED_TYPE WS2812
#define COLOR_ORDER GRB

CHSV strip1Color = cblue;
CHSV strip2Color = cblue;

#if defined(__CO2__)
const int SCULPTURE_ID = 1;
int readings1[17], readings2[40];
CRGB leds0[CO2band1_1]; CRGB leds1[CO2band1_2]; CRGB leds2[CO2band1_3]; CRGB leds3[CO2band2];

#elif defined(__PM25__)
const int SCULPTURE_ID = 2;
int readings1[20], readings2[32];
CRGB leds0[PM25band1]; CRGB leds1[PM25band2]; CRGB leds2[0]; CRGB leds3[0];

#elif defined(__VOC__)
const int SCULPTURE_ID = 3;
int readings1[26], readings2[22];
CRGB leds0[VOCband1]; CRGB leds1[VOCband2]; CRGB leds2[0]; CRGB leds3[0];

#else
#error "invalid sculpture ID"
#endif

#define UPDATES_PER_SECOND 100 //speed of light animation
const int IDLE_MODE = 1, BUTTON_MODE = 2;
unsigned int strip1playMode = IDLE_MODE, strip2playMode = IDLE_MODE; 

int strip1brightness = 0, strip2brightness = 0; //band brightness
int strip1maxBrightLvl = 255, strip2maxBrightLvl = 255; //variable max brightness
bool strip1hasPlayModeChanged = false, strip2hasPlayModeChanged = false; //for audio track changes
int strip1activeLedState = 0, strip2activeLedState = 0;            //to track led animaton states, e.g. 0 - idle mode, start fade to black 1 - show brightness according to reading, 2 - has completed animations, fade to black and idle
bool strip1isMaxBrightness = false, strip2isMaxBrightness = false;      //to track idle animation direction
elapsedMillis strip1bandms, strip2bandms;              //multiple use time ellapsed counter
unsigned int strip1bandDelay = BAND_DELAY, strip2bandDelay = BAND_DELAY; //speed of fade animation
unsigned int strip1readingsCounter, strip2readingsCounter;                 //keeps track of indexing the readings array
unsigned int strip1prevBrightVal, strip1currBrightVal, strip2prevBrightVal, strip2currBrightVal;    //for comparing prev and current values for dimming and brightening

#include "myfunctions.h" //supporting functions

//-------------------- Setup --------------------//

void setup() {

  pinMode(button0pin, INPUT_PULLUP);
  pinMode(button1pin, INPUT_PULLUP);

  Serial.begin(9600);

  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }

  delay(2000); //power up safety delay

  if (SCULPTURE_ID == 1) //top ring of CO2 sculpture split into 3 strips
  {
    FastLED.addLeds<LED_TYPE, CO2STRIP1_1PIN, COLOR_ORDER>(leds0, CO2band1_1);
    FastLED.addLeds<LED_TYPE, CO2STRIP1_2PIN, COLOR_ORDER>(leds1, CO2band1_2);
    FastLED.addLeds<LED_TYPE, CO2STRIP1_3PIN, COLOR_ORDER>(leds2, CO2band1_3);
    FastLED.addLeds<LED_TYPE, CO2STRIP2PIN, COLOR_ORDER>(leds3, CO2band2);
  } 
  else if (SCULPTURE_ID == 2)
  {
    FastLED.addLeds<LED_TYPE, STRIP1PIN, COLOR_ORDER>(leds0, PM25band1);
    FastLED.addLeds<LED_TYPE, STRIP2PIN, COLOR_ORDER>(leds1, PM25band2); 
  }
  else if (SCULPTURE_ID == 3)
  {
    FastLED.addLeds<LED_TYPE, STRIP1PIN, COLOR_ORDER>(leds0, VOCband1);
    FastLED.addLeds<LED_TYPE, STRIP2PIN, COLOR_ORDER>(leds1, VOCband2); 
  }
  FastLED.setBrightness(255);

  delay(10);

  register_readings(); //translate the air measurement data points into a readings[] brightness value array
}

void loop() {
  read_console();//gets input from dist sensor and buttons

  do_colour_variation();//changes hue of both strips according to dist sensor

  set_playMode();

  if (strip1playMode == IDLE_MODE)
  {
    strip1_idle_animation();
  }
  else if (strip1playMode == BUTTON_MODE)
  {
    strip1_playback_readings(); //play brightness sequence according to readings[] array
  }

  if (strip2playMode == IDLE_MODE)
  {
    strip2_idle_animation();
  } 
  else if (strip2playMode == BUTTON_MODE)
  {
    strip2_playback_readings(); //play brightness sequence according to readings[] array
  }

  add_glitter();

  FastLED.show();
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}

