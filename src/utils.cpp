#include "utils.h"

void pico_set_led(bool led_on)
{
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
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