#include "pico/stdlib.h"
#include "pico/rand.h"
#include <stdio.h>

#define BUTTON_A_PIN 15
#define BUTTON_B_PIN 14
#define BUTTON_X_PIN 13
#define BUTTON_Y_PIN 12
#define BUTTON_LB_PIN 10
#define BUTTON_RB_PIN 11

// IO Methods
int pico_led_init();
void init_pin(int pin);
void pico_set_led(bool led_on);
bool is_button_pressed(int button);
void loop_blink(int times, int sleep);

// General Methods
uint8_t bools_to_uint8(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7);
void generate_random_addr(uint8_t *output_buffer);