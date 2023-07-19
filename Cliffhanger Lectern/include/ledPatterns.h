#ifndef LED_PATTERNS_H
#define LED_PATTERNS_H



#define PIN      14
#define N_LEDS   39

void initLEDs();
void ledLoop();

void startWinPattern();
void startLosePattern();
void startWarningPattern();

#endif