#include <RF24.h>

#include "tusb.h"
#include "XInputPad.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "utils.h"
#include "comms/comms.h"

#define CE_PIN 20

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

    // Init i2c
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);


    // Init Radio
    // spi.begin(spi0, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_RX_PIN);
    // if (!radio.begin(&spi))
    // {
    //     while (true)
    //     {
    //         loop_blink(1, 100);
    //         sleep_ms(100);
    //     }
    // }
    
    
    // Set board type
    bool isTransmitter = true;
    
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
