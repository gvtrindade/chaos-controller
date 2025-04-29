#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tusb.h"
#include "hardware/spi.h"

#include <RF24.h>
#include "XInputPad.h"


#define CE_PIN 1
#define BUTTON_A_PIN 15
#define BUTTON_B_PIN 14
#define BUTTON_X_PIN 13
#define BUTTON_Y_PIN 12
#define BUTTON_RB_PIN 11

uint8_t payload = 0;

void master();
void slave();
int pico_led_init();
void pico_set_led(bool led_on);
void loop_blink();
void init_pin(int pin);
bool is_button_pressed(int button);
uint8_t bools_to_uint8(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7);

XInputReport XboxButtonData = {
    .report_id = 0,
    .report_size = XINPUT_ENDPOINT_SIZE,
    .buttons1 = 0,
    .buttons2 = 0,
    .lt = 0,
    .rt = 0,
    .lx = GAMEPAD_JOYSTICK_MID,
    .ly = GAMEPAD_JOYSTICK_MID,
    .rx = GAMEPAD_JOYSTICK_MID,
    .ry = GAMEPAD_JOYSTICK_MID,
    ._reserved = {},
};

RF24 radio(CE_PIN, PICO_DEFAULT_SPI_CSN_PIN);
SPI spi;

int main()
{
    stdio_init_all();
    pico_led_init();
    tusb_init();

    spi.begin(spi0, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_RX_PIN);
    if (!radio.begin(&spi))
    {
        loop_blink();
    }

    uint8_t address[2][6] = {"1Node", "2Node"};
    bool radioNumber = 1;

    radio.setPayloadSize(sizeof(payload));
    radio.setPALevel(RF24_PA_LOW);
    radio.openWritingPipe(address[radioNumber]);
    radio.openReadingPipe(1, address[!radioNumber]);

    bool is_slave = radioNumber;
    if (is_slave)
    {
        slave();
    }
    else
    {
        master();
    }
}

void master()
{
    radio.stopListening();

    unsigned int failures = 0;
    uint8_t prev_state = 0;

    while (true)
    {
        uint8_t state = bools_to_uint8(
            0, !gpio_get(BUTTON_RB_PIN),
            0, 0,
            !gpio_get(BUTTON_A_PIN), !gpio_get(BUTTON_B_PIN),
            !gpio_get(BUTTON_X_PIN), !gpio_get(BUTTON_Y_PIN)
        );

        if (state != prev_state)
        {
            bool report = radio.write(&state, sizeof(payload));
            pico_set_led(state);

            if (!report)
            {
                failures++;
            }
        }

        prev_state = state;
    }
}

void slave()
{
    radio.startListening();

    uint8_t prev_state = 0;
    while (true)
    {
        uint8_t pipe;
        if (radio.available(&pipe))
        {
            uint8_t bytes = radio.getPayloadSize();
            radio.read(&payload, bytes);    
            
            if (payload != prev_state)
            {
                pico_set_led(payload);
                XboxButtonData.buttons2 = payload;
            }
        }

        sendReportData(&XboxButtonData);
        tud_task();
        prev_state = payload;
    }

    radio.stopListening();
}

int pico_led_init(void)
{
    init_pin(BUTTON_A_PIN);
    init_pin(BUTTON_B_PIN);
    init_pin(BUTTON_X_PIN);
    init_pin(BUTTON_Y_PIN);
    init_pin(BUTTON_RB_PIN);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

void pico_set_led(bool led_on)
{
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

void loop_blink()
{
    while (true)
    {
        pico_set_led(true);
        sleep_ms(100);
        pico_set_led(false);
        sleep_ms(100);
    }
}

bool is_button_pressed(int button)
{
    return !gpio_get(button);
}

void init_pin(int pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
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