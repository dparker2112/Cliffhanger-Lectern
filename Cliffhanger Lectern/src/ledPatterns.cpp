#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include "ledPatterns.h"


//grbw adjust sequenct if colors are wrong -- i.e. rbgw
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, PIN, NEO_RGB + NEO_KHZ800);

#define MAX_INTENSITY  255

typedef enum led_pattern_t {
  LED_WIN,
  LED_LOSE,
  LED_WARNING,
  LED_OFF
} led_pattern_t;


typedef enum led_pattern_state_t {
  FADE_IN,
  CROSSFADE,
  SOLID,
  HOLD,
  FLICKER,
  ALTERNATE_COLORS,
  THEATER_CHASE,
  FADE_OUT,
  DONE
} led_pattern_state_t;


//variables for lose state
uint32_t off = 0x00000000;

typedef struct led_pattern_data_t {
  led_pattern_state_t state;
  uint16_t fadeInLength;
  uint16_t fadeOutLength;
  uint16_t interval1;
  uint16_t interval2;
  uint16_t interval3;
  uint32_t color1;
  uint32_t color2;
  uint8_t count1;
  uint8_t count2;
}led_pattern_data_t;



led_pattern_data_t winPattern;
led_pattern_data_t losePattern;
led_pattern_data_t warningPattern;


unsigned long startTime;


led_pattern_t ledPatternState = LED_OFF;
int16_t progress = 0;
int flickerCounter = 0;

int ledWinNoBlock();
int ledLoseNoBlock();
int ledWarningNoBlock();
void setFadeOutColor(uint32_t color, int i);
void setFadeInColor(uint32_t color, int i);
void setCrossFadeColor(uint32_t color1, uint32_t color2, int progress);
void setSolidColor(uint32_t color);
void setColor(uint32_t c);
void clearBuffer();

uint8_t getR(uint32_t color);
uint8_t getG(uint32_t color);
uint8_t getB(uint32_t color);
uint8_t getW(uint32_t color);
int theaterChaseCount;

uint16_t flickerOnTimes[] = {50, 10, 600, 100, 20,40,25};
uint16_t flickerOffTimes[] = {200, 150, 30, 200, 80,30,60};

uint32_t patternStart;

void initLEDs() {
  strip.begin();

  winPattern.state = FADE_IN;
  winPattern.fadeInLength = 300;   //how long it takes to fade in
  winPattern.fadeOutLength = 4000;  //how long it takes to fade out
  winPattern.interval1 = 137;       //length of bells in ms
  winPattern.count1 = 34;           //number of rings of bell
  winPattern.interval2 = 2000;      //how long transition takes
  winPattern.interval3 = 1000;      //how long it holds before fading out
  winPattern.color1 = 0x0000FF00; // Initial color (green).
  winPattern.color2 = 0x000000FF; // Target color (blue).

  flickerCounter = sizeof(flickerOffTimes)/sizeof(flickerOffTimes[0]);
  losePattern.state = FADE_IN;
  losePattern.color1 = 0x00FF0000;
  losePattern.interval1 = 1000;


  warningPattern.state = FADE_IN;
  warningPattern.fadeInLength = 5;   //ms per fade interval
  warningPattern.fadeOutLength = 5;  //how long it takes to fade out
  warningPattern.color1 = 0x007F7F00;
  warningPattern.color2 = 0x00FF0000;
  warningPattern.count1 = 0;
  warningPattern.count2 = 4;        //the number of blinks between the colors
  warningPattern.interval1 = 400;
  warningPattern.interval2 = 50; //speed of chase
  uint16_t theaterChaseLength = 3000; //seconds for theater chase
  theaterChaseCount = theaterChaseLength/warningPattern.interval2; // The number of iterations for the theater chase
}



void printPatternLength() {
  Serial.print("pattern length: ");
  Serial.print((float)(millis() - patternStart)/1000.0);
  Serial.println("s");
}

void ledLoop(){
  switch(ledPatternState) {
    case LED_WIN:
      if(ledWinNoBlock() < 0) {
        ledPatternState = LED_OFF;
        printPatternLength();

      }
      break;
    case LED_LOSE:
      if(ledLoseNoBlock() < 0) {
        ledPatternState = LED_OFF;
        printPatternLength();
      }

      break;
    case LED_WARNING:
      if(ledWarningNoBlock() < 0) {
        ledPatternState = LED_OFF;
        printPatternLength();
      }

      break;
    case LED_OFF:

      break;
  }
}

void startWinPattern() {
  ledPatternState = LED_WIN;
  patternStart = millis();
  progress = 0;
}
void startLosePattern() {
  ledPatternState = LED_LOSE;
  patternStart = millis();
  progress = 0;
}
void startWarningPattern() {
  ledPatternState = LED_WARNING;
  patternStart = millis();
  progress = 0;
}



int ledWinNoBlock() {
  unsigned long currentMillis = millis();

  switch (winPattern.state) {
    case FADE_IN:
      if (currentMillis - startTime < winPattern.fadeInLength) {
        int i = map(currentMillis - startTime, 0, winPattern.fadeInLength, 0, 255);
        setFadeInColor(winPattern.color1, i);
      } else {
        progress = 0;
        winPattern.state = ALTERNATE_COLORS;
        startTime = currentMillis;
      }
      break;
    case ALTERNATE_COLORS:
    
    if (currentMillis - startTime > winPattern.interval1) {
      startTime = currentMillis;
      uint32_t color1, color2;
      if(progress%2 == 0) {
        color1 = winPattern.color1;
        color2 = winPattern.color2;
      } else {
        color1 = winPattern.color2;
        color2 = winPattern.color1;
      }
      progress++;
      int chunkSize = N_LEDS/5;
      for(int i = 0; i < N_LEDS; i = i + chunkSize) {
        for(int j = 0; j < chunkSize; j++) {
          uint32_t color;
          if(i%2 == 0) {
            color = color1;
          } else {
            color = color2;
          }
          strip.setPixelColor(i + j, color);
        }

      }
      if(progress > winPattern.count1) {
        progress = 0;
        winPattern.state = CROSSFADE;
      }
    }
    break;
    case CROSSFADE:
      if (currentMillis - startTime < winPattern.interval1) {
        progress = map(currentMillis - startTime, 0, winPattern.interval1, 0, 255);
        setCrossFadeColor(winPattern.color1, winPattern.color2, progress);
      } else {
        setSolidColor(winPattern.color2);
        winPattern.state = SOLID;
        startTime = currentMillis;
      }
      break;

    case SOLID:
      if (currentMillis - startTime >= winPattern.interval2) {
        progress = 255;
        winPattern.state = FADE_OUT;
        startTime = currentMillis;
      }
      break;

    case FADE_OUT:
      if (currentMillis - startTime < winPattern.fadeOutLength) {
        int i = map(currentMillis - startTime, 0, winPattern.fadeOutLength, 255, 0);
        setFadeOutColor(winPattern.color2, i);
      } else {
        winPattern.state = DONE;
        Serial.println("done");
        startTime = currentMillis;
      }
      break;
    case DONE:
      winPattern.state = FADE_IN;
      setColor(off);
      return -1;
      break;
  }

  strip.show();
  return 0;
}



enum FlickerState {
  ON,
  OFF
};

FlickerState flickerState = ON;
int flickerIndex = 0;
int ledLoseNoBlock() {
  unsigned long currentMillis = millis();

  switch(losePattern.state) {
    case FADE_IN:
      if (progress < 256) {
        uint8_t r1 = getR(losePattern.color1);
        uint8_t g1 = getG(losePattern.color1);
        uint8_t b1 = getB(losePattern.color1);
        setColor(strip.Color((float)r1 * progress / MAX_INTENSITY, (float)g1 * progress / MAX_INTENSITY, (float)b1 * progress / MAX_INTENSITY));
        progress++;
        startTime = currentMillis;
      } else {
        losePattern.state = HOLD;
        startTime = currentMillis;
      }
      break;

    case HOLD:
      if (currentMillis - startTime >= losePattern.interval1) {
        losePattern.state = FLICKER;
        flickerIndex = 0;
        startTime = currentMillis;
      }
      break;

    case FLICKER:
      if (flickerIndex < flickerCounter) {
        if (flickerState == ON && currentMillis - startTime >= flickerOnTimes[flickerIndex]) {
          setColor(off);
          Serial.println("off");
          startTime = currentMillis;
          flickerState = OFF;
        } else if (flickerState == OFF && currentMillis - startTime >= flickerOffTimes[flickerIndex]) {
          setColor(losePattern.color1);
          Serial.println("on");
          startTime = currentMillis;
          flickerState = ON;
          flickerIndex++;
        }
      } else {
        losePattern.state = DONE;
      }
      break;

    case DONE:
      Serial.println("done");
      setColor(off);
      strip.show();
      losePattern.state = FADE_IN;
      return -1;
      break;
  }
  strip.show();
  return 0;
}


int ledWarningNoBlock() {
  unsigned long currentMillis = millis();
  static unsigned long warningStartTime = currentMillis;
  static int theaterChaseIndex = 0;
  switch (warningPattern.state) {
    case FADE_IN:
      if (currentMillis - warningStartTime >= warningPattern.fadeInLength) {
        uint8_t r1 = getR(warningPattern.color1);
        uint8_t g1 = getG(warningPattern.color1);
        uint8_t b1 = getB(warningPattern.color1);
        setColor(strip.Color((float)r1 * progress / MAX_INTENSITY, (float)g1 * progress / MAX_INTENSITY, (float)b1 * progress / MAX_INTENSITY));
        progress++;
        warningStartTime = currentMillis;
        if (progress >= 256) {
          warningPattern.state = ALTERNATE_COLORS;
          theaterChaseIndex = 0;
          warningPattern.count1 = 0;
          progress = 0;
        }
      }
      break;

    case ALTERNATE_COLORS:
      if (currentMillis - warningStartTime >= warningPattern.interval1) {
        setColor(warningPattern.count1 % 2 == 0 ? warningPattern.color1 : warningPattern.color2);
        warningPattern.count1++;
        warningStartTime = currentMillis;
        if (warningPattern.count1 >= warningPattern.count2 * 2) {
          warningPattern.state = THEATER_CHASE;
          theaterChaseIndex = 0;
        }
      }
      break;

    case THEATER_CHASE:
      if (currentMillis - warningStartTime >= warningPattern.interval2) {
        clearBuffer();
        for (int i = 0; i < strip.numPixels(); i++) {
          if ((i + theaterChaseIndex) % 3 == 0) {
            strip.setPixelColor(i, theaterChaseIndex < theaterChaseCount - N_LEDS ? warningPattern.color1 : warningPattern.color2);
          } else {
            strip.setPixelColor(i, warningPattern.color2);
          }
        }
        strip.show();
        theaterChaseIndex++;
        warningStartTime = currentMillis;
        if (theaterChaseIndex >= theaterChaseCount) {
          warningPattern.state = FADE_OUT;
          progress = 255;
        }
      }
      break;

    case FADE_OUT:
      if (currentMillis - warningStartTime >= warningPattern.fadeOutLength) {
        uint8_t r1 = getR(warningPattern.color2);
        uint8_t g1 = getG(warningPattern.color2);
        uint8_t b1 = getB(warningPattern.color2);
        setColor(strip.Color((float)r1 * progress / MAX_INTENSITY, (float)g1 * progress / MAX_INTENSITY, (float)b1 * progress / MAX_INTENSITY));
        progress--;
        warningStartTime = currentMillis;
        if (progress < 0) {
          warningPattern.state = DONE;
        }
      }
      break;
    case DONE:
      warningPattern.state = FADE_IN;
      setColor(off);
      return -1;
      break;
  }
  return 0;
}


uint8_t getR(uint32_t color) {
  return color >> 16; 
}

uint8_t getG(uint32_t color) {
  return color >> 8; 
}

uint8_t getB(uint32_t color) {
  return color & 0xFF; 
}

uint8_t getW(uint32_t color) {
  return color >> 24; 
}

void setColor(uint32_t c) {
  //Serial.println(c,HEX);
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}

void clearBuffer() {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);
  }
}

void setFadeOutColor(uint32_t color, int i) {
  uint8_t r2 = getR(color);
  uint8_t g2 = getG(color);
  uint8_t b2 = getB(color);
  uint32_t fadedColor = strip.Color((float)r2 * i / MAX_INTENSITY, (float)g2 * i / MAX_INTENSITY, (float)b2 * i / MAX_INTENSITY);
  //Serial.println(fadedColor,HEX);
  for(int j = 0; j < N_LEDS; j++) {
    strip.setPixelColor(j, fadedColor);
  }
}

void setFadeInColor(uint32_t color, int i) {
  uint8_t r1 = getR(color);
  uint8_t g1 = getG(color);
  uint8_t b1 = getB(color);
  setColor(strip.Color((float)r1 * i / MAX_INTENSITY, (float)g1 * i / MAX_INTENSITY, (float)b1 * i / MAX_INTENSITY));
}

void setCrossFadeColor(uint32_t color1, uint32_t color2, int progress) {
  for(int i = 0; i < N_LEDS; i++) {
    uint8_t r = (255.0 - progress) * (float)getR(color1)/255.0 + progress * (float)getR(color2)/255.0;
    uint8_t g = (255.0 - progress) * (float)getG(color1)/255.0 + progress * (float)getG(color2)/255.0;
    uint8_t b = (255.0 - progress) * (float)getB(color1)/255.0 + progress * (float)getB(color2)/255.0;
    strip.setPixelColor(i, strip.Color(r,g,b));
  }
}

void setSolidColor(uint32_t color) {
  for(int i = 0; i < N_LEDS; i++) {
    strip.setPixelColor(i, color);
  }
}