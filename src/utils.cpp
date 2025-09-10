#include "utils.h"

int pico_led_init(void)
{
    init_pin(BUTTON_SYNC_PIN);
    init_pin(BUTTON_WHA_PIN);
    init_pin(BUTTON_SGRE_PIN);
    init_pin(BUTTON_SRED_PIN);
    init_pin(BUTTON_SYEL_PIN);
    init_pin(BUTTON_SBLU_PIN);
    init_pin(BUTTON_SORA_PIN);
    init_pin(PLAY1_LED_PIN);
    init_pin(PLAY2_LED_PIN);
    init_pin(PLAY3_LED_PIN);
    init_pin(PLAY4_LED_PIN);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

void init_pin(int pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

bool is_button_pressed(int button)
{
    return !gpio_get(button);
}

void loop_blink(int times, int sleep)
{
    for (int i = 0; i < times; i++) {
        pico_set_led(true);
        sleep_ms(sleep);
        pico_set_led(false);
        sleep_ms(sleep);
    }
}

void pico_set_led(bool led_on)
{
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

uint8_t bools_to_uint8(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7)
{
    uint8_t result = 0;
    if (b0)
        result |= (1 << 0);
    if (b1)
        result |= (1 << 1);
    if (b2)
        result |= (1 << 2);
    if (b3)
        result |= (1 << 3);
    if (b4)
        result |= (1 << 4);
    if (b5)
        result |= (1 << 5);
    if (b6)
        result |= (1 << 6);
    if (b7)
        result |= (1 << 7);
    return result;
}

void update_special_color_button(int pin, uint8_t* buttons1_state, uint8_t* buttons2_state, int bit) {
    if (!gpio_get(pin)) {
        *buttons1_state |= (1 << 0);
        *buttons2_state |= (1 << bit);
    }
}

const char CHARACTER_SET[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const int CHARACTER_SET_LENGTH = sizeof(CHARACTER_SET) - 1;

void generate_random_addr(uint8_t *output_buffer) {
    output_buffer[0] = 65;
    for (int i = 1; i < 5; i++) {
        int random_index = get_rand_32() % CHARACTER_SET_LENGTH;
        printf("Receiver: Char: %i\n", random_index);
        output_buffer[i] = CHARACTER_SET[random_index];
    }
    output_buffer[5] = 0;
}