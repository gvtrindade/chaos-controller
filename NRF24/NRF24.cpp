#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <RF24.h>
#include <RF24Mesh.h>
#include <RF24Network.h>

#define CE_PIN 1
#define BUTTON_PIN 15

bool payload = 0;

void master();
void slave();
int pico_led_init();
void loop_blink();
void pico_set_led(bool led_on);

RF24 radio(CE_PIN, PICO_DEFAULT_SPI_CSN_PIN);
SPI spi;

int main()
{
    stdio_init_all();
    pico_led_init();

    spi.begin(spi0, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_RX_PIN);
    if (!radio.begin(&spi)) {
        loop_blink();
    }

    uint8_t address[2][6] = {"1Node", "2Node"};
    bool radioNumber = 1;

    radio.setPayloadSize(sizeof(payload));
    radio.setPALevel(RF24_PA_LOW);
    radio.openWritingPipe(address[radioNumber]);
    radio.openReadingPipe(1, address[!radioNumber]);

    bool is_slave = radioNumber;
    if (is_slave) {
        slave();
    } else {
        master();
    }

}

void master() {
    radio.stopListening();

    unsigned int failures = 0;
    bool prev_state = false;
    while (true) {
        bool state = !gpio_get(BUTTON_PIN);

        if (state != prev_state) {
            bool report = radio.write(&state, sizeof(payload));
            pico_set_led(state);

            if (!report) {
                failures++;
            }
        }

        prev_state = state;
        delay(10);
    }
}

void slave() {
    radio.startListening();

    while(true) {
        uint8_t pipe;
        if (radio.available(&pipe)) {
            uint8_t bytes = radio.getPayloadSize();
            radio.read(&payload, bytes);
            pico_set_led(payload);
        }
    }

    radio.stopListening();
}

int pico_led_init(void) {
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

void loop_blink() {
    while(true) {
        pico_set_led(true);
        sleep_ms(100);
        pico_set_led(false);
        sleep_ms(100);
    }
}

void pico_set_led(bool led_on) {
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}