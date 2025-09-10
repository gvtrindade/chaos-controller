#include "pico/stdlib.h"
#include "pico/rand.h"
#include <stdio.h>

// Pico pins
#define BUTTON_SYNC_PIN 3
#define BUTTON_WHA_PIN 6
#define BUTTON_SGRE_PIN 7
#define BUTTON_SRED_PIN 8
#define BUTTON_SYEL_PIN 9
#define BUTTON_SBLU_PIN 10
#define BUTTON_SORA_PIN 11

#define PLAY1_LED_PIN 12
#define PLAY2_LED_PIN 13
#define PLAY3_LED_PIN 14
#define PLAY4_LED_PIN 15

// IO Methods
int pico_led_init();
void init_pin(int pin);
void pico_set_led(bool led_on);
bool is_button_pressed(int button);
void loop_blink(int times, int sleep);

// General Methods
void update_special_color_button(int pin, uint8_t* buttons1_state, uint8_t* buttons2_state, int bit);
void generate_random_addr(uint8_t *output_buffer);