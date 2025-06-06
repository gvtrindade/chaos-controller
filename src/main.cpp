#include <RF24.h>

#include "tusb.h"
#include "XInputPad.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "utils.h"
#include "comms/comms.h"

#define CE_PIN 2

uint8_t payload = 0;

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
    // Init IO and USB
    stdio_init_all();
    pico_led_init();
    tusb_init();
    
    // Init Radio
    spi.begin(spi0, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_RX_PIN);
    if (!radio.begin(&spi))
    {
        while (true)
        {
            loop_blink(1, 100);
            sleep_ms(100);
        }
    }
    
    radio.setPALevel(RF24_PA_LOW);
    radio.enableDynamicPayloads();
    radio.enableAckPayload();
    
    // Set board type
    bool isTransmitter = false;
    
    if (isTransmitter)
    {
        setup_transmitter(radio);
        loop_transmitter(radio, buttonData);
    }
    else
    {
        tud_disconnect();
        setup_receiver(radio);
        loop_receiver(radio, buttonData);
    }
}
