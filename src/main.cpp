#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tusb.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include <RF24.h>
#include "XInputPad.h"
#include "comms.h"
#include "utils.h"

#define CE_PIN 2

uint8_t payload = 0;

void transmitter();
void receiver();
int pico_led_init();
void pico_set_led(bool led_on);
void init_pin(int pin);
bool is_button_pressed(int button);
uint8_t bools_to_uint8(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7);

XInputReport buttonData = {
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

    bool is_transmitter = false;
    if (!is_transmitter)
    {
        tud_disconnect();
    }

    spi.begin(spi0, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_RX_PIN);
    if (!radio.begin(&spi))
    {
        while (true)
        {
            loop_blink(1, 100);
            sleep_ms(100);
        }
    }

    radio.enableDynamicPayloads();
    radio.setPALevel(RF24_PA_LOW);
    radio.enableAckPayload();

    if (is_transmitter)
    {
        transmitter();
    }
    else
    {
        receiver();
    }
}

void transmitter()
{
    setup_transmitter(radio);
    loop_transmitter(radio, buttonData);

    // radio.stopListening();
    // unsigned int failures = 0;
    // uint8_t prev_state = 0;

    // while (true)
    // {
    //     tud_task();

    //     uint8_t state = bools_to_uint8(
    //         0, !gpio_get(BUTTON_RB_PIN),
    //         0, 0,
    //         !gpio_get(BUTTON_A_PIN), !gpio_get(BUTTON_B_PIN),
    //         !gpio_get(BUTTON_X_PIN), !gpio_get(BUTTON_Y_PIN));

    //     if (state != prev_state)
    //     {
    //         buttonData.buttons2 = state;

    //         if (tud_suspended())
    //         {
    //             // Wake up host if we are in suspend mode
    //             // and REMOTE_WAKEUP feature is enabled by host
    //             tud_remote_wakeup();
    //         }

    //         if (tud_ready())
    //         {
    //             sendReportData(&buttonData);
    //         }
    //         else
    //         {
    //             bool report = radio.write(&state, sizeof(payload));
    //             pico_set_led(state);

    //             if (!report)
    //             {
    //                 failures++;
    //             }
    //         }
    //     }

    //     prev_state = state;
    // }
}

void receiver()
{
    setup_receiver(radio);
    loop_blink(2, 80);
    loop_receiver(radio, buttonData);

    // radio.startListening();

    // uint8_t prev_state = 0;
    // while (true)
    // {
    //     tud_task();

    //     uint8_t pipe;
    //     if (radio.available(&pipe))
    //     {
    //         uint8_t bytes = radio.getPayloadSize();
    //         radio.read(&payload, bytes);

    //         if (payload != prev_state)
    //         {
    //             pico_set_led(payload);
    //             buttonData.buttons2 = payload;
    //         }
    //     }

    //     sendReportData(&buttonData);
    //     prev_state = payload;
    // }

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
