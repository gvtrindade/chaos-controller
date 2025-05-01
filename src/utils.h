#include "pico/stdlib.h"

#define BUTTON_A_PIN 15
#define BUTTON_B_PIN 14
#define BUTTON_X_PIN 13
#define BUTTON_Y_PIN 12
#define BUTTON_RB_PIN 11

void pico_set_led(bool led_on);
void loop_blink(int times, int sleep);
uint8_t bools_to_uint8(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7);
