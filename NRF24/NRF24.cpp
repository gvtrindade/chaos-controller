#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tusb.h"
#include "device/usbd_pvt.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <RF24.h>
#include <RF24Mesh.h>
#include <RF24Network.h>

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
void loop_blink();
void pico_set_led(bool led_on);

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

static inline uint32_t board_millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

uint8_t endpoint_in = 0;
uint8_t endpoint_out = 0;

static void sendReportData(void *report)
{

    // Poll every 1ms
    const uint32_t interval_ms = 1;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms)
        return; // not enough time
    start_ms += interval_ms;

    // Remote wakeup
    if (tud_suspended())
    {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    }

    if ((tud_ready()) && ((endpoint_in != 0)) && (!usbd_edpt_busy(0, endpoint_in)))
    {
        usbd_edpt_claim(0, endpoint_in);
        usbd_edpt_xfer(0, endpoint_in, (uint8_t *)report, XINPUT_ENDPOINT_SIZE);
        usbd_edpt_release(0, endpoint_in);
    }
}

bool is_button_pressed(int button)
{
    return !gpio_get(button);
}

void init_pin(int pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
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

static void xinput_init(void)
{
}

static void xinput_reset(uint8_t __unused rhport)
{
}

static uint16_t xinput_open(uint8_t __unused rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
{
    //+16 is for the unknown descriptor
    uint16_t const drv_len = sizeof(tusb_desc_interface_t) + itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t) + 16;
    TU_VERIFY(max_len >= drv_len, 0);

    uint8_t const *p_desc = tu_desc_next(itf_desc);
    uint8_t found_endpoints = 0;
    while ((found_endpoints < itf_desc->bNumEndpoints) && (drv_len <= max_len))
    {
        tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;
        if (TUSB_DESC_ENDPOINT == tu_desc_type(desc_ep))
        {
            TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

            if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN)
            {
                endpoint_in = desc_ep->bEndpointAddress;
            }
            else
            {
                endpoint_out = desc_ep->bEndpointAddress;
            }
            found_endpoints += 1;
        }
        p_desc = tu_desc_next(p_desc);
    }
    return drv_len;
}

static bool xinput_device_control_request(uint8_t __unused rhport, uint8_t __unused stage, tusb_control_request_t __unused const *request)
{
    return true;
}

static bool xinput_control_complete_cb(uint8_t __unused rhport, tusb_control_request_t __unused const *request)
{
    return true;
}
// callback after xfer_transfer
static bool xinput_xfer_cb(uint8_t __unused rhport, uint8_t __unused ep_addr, xfer_result_t __unused result, uint32_t __unused xferred_bytes)
{
    return true;
}

static usbd_class_driver_t const xinput_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "XINPUT",
#endif
    .init = xinput_init,
    .reset = xinput_reset,
    .open = xinput_open,
    .control_xfer_cb = xinput_device_control_request,
    .xfer_cb = xinput_xfer_cb,
    .sof = NULL};

// Implement callback to add our custom driver
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &xinput_driver;
}

uint8_t bools_to_uint8(bool b0, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7) {
    uint8_t result = 0;
    if (b0) result |= (1 << 0);
    if (b1) result |= (1 << 1);
    if (b2) result |= (1 << 2);
    if (b3) result |= (1 << 3);
    if (b4) result |= (1 << 4);
    if (b5) result |= (1 << 5);
    if (b6) result |= (1 << 6);
    if (b7) result |= (1 << 7);
    return result;
}

// ==================================================================================
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
    bool radioNumber = 0;

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