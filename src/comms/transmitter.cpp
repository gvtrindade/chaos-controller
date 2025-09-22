#include "comms.h"
#include "i2c.h"

// Methods
void setup_transmitter(RF24 radio);
void loop_transmitter(RF24 radio, XInputReport buttonData);
void send_data(RF24 radio, XInputReport buttonData);
void pair(RF24 radio);
void confirm(RF24 radio, uint8_t conf_addr[6]);
void save_new_id(ConfirmPacket confirmPacket);

// Enums
enum TxState
{
    UNPAIRED,
    WAITING_FOR_ACK,
    PAIRED
};
TxState current_state = UNPAIRED;

// Contants
const unsigned long RETRY_DELAY = 2000; // ms

// Variables
uint32_t timeSinceLastAttempt = 0;
uint32_t unpairedTimer = 0;
uint16_t my_id = 0;
bool isFirstExecution = true;
uint8_t coms_addr[6];
int tries = 0;

bool flDataSent = false;

static inline uint32_t board_millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void setup_transmitter(RF24 radio)
{
    // Get last saved id
    // my_id = get_saved_id();
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);

    radio.setRetries(5, 15);
    printf("Transmitter: Starting transmitter module\n");
}



void loop_transmitter(RF24 radio, XInputReport buttonData)
{
    uint8_t b1_prev_state = 0;
    uint8_t b2_prev_state = 0;

    int spec_buttons[5] = {
        BUTTON_SGRE_PIN, BUTTON_SRED_PIN, BUTTON_SYEL_PIN,
        BUTTON_SBLU_PIN, BUTTON_SORA_PIN
    };
    int bits[5] = {4, 5, 6, 7, 1};

    while (true)
    {
        // Update USB stack
        tud_task();

        // Get data from chips
        uint8_t received_data[NUM_CHIPS];

        // Read data from each chip
        for (int i = 0; i < NUM_CHIPS; i++) {
            int result = i2c_read_blocking(I2C_PORT, CHIP_ADDRS[i], &received_data[i], 1, false);

            if (result == PICO_ERROR_GENERIC) {
                printf("Error: No device found at address 0x%02X\n", CHIP_ADDRS[i]);
                received_data[i] = 0xFF; // Set to default state on error
            }

        }

        uint8_t buttons1_state = 0;
        uint8_t buttons2_state = 0;
        
        // Face Buttons, Strum acts as UP or DOWN
        if (!(received_data[0] & BUTTON_UP_ADDR))    buttons1_state |= (1 << 0); // Set bit 0
        if (!(received_data[1] & BUTTON_STRU_ADDR))  buttons1_state |= (1 << 0); // Set bit 0
        if (!(received_data[0] & BUTTON_DOWN_ADDR))  buttons1_state |= (1 << 1); // Set bit 1
        if (!(received_data[1] & BUTTON_STRD_ADDR))  buttons1_state |= (1 << 1); // Set bit 1
        if (!(received_data[0] & BUTTON_LEFT_ADDR))  buttons1_state |= (1 << 2); // Set bit 2
        if (!(received_data[0] & BUTTON_RIGHT_ADDR)) buttons1_state |= (1 << 3); // Set bit 3

        // Action Buttons
        if (!(received_data[0] & BUTTON_SELEC_ADDR)) buttons1_state |= (1 << 4); // Set bit 4
        if (!(received_data[0] & BUTTON_START_ADDR)) buttons1_state |= (1 << 5); // Set bit 5
        if (!(received_data[0] & BUTTON_XCE_ADDR))  buttons2_state |= (1 << 2); // Set bit 1

        // Color buttons
        if (!(received_data[1] & BUTTON_ORA_ADDR))   buttons2_state |= (1 << 1); // Set bit 1
        if (!(received_data[1] & BUTTON_GRE_ADDR))   buttons2_state |= (1 << 4); // Set bit 4
        if (!(received_data[1] & BUTTON_RED_ADDR))   buttons2_state |= (1 << 5); // Set bit 5
        if (!(received_data[1] & BUTTON_YEL_ADDR))   buttons2_state |= (1 << 6); // Set bit 6
        if (!(received_data[1] & BUTTON_BLU_ADDR))   buttons2_state |= (1 << 7); // Set bit 7

        // Special color buttons
        for (int i = 0; i < 5; i++) {
            update_special_color_button(spec_buttons[i], &buttons1_state, &buttons2_state, bits[i]);
        }

        if (buttons1_state != b1_prev_state) {
            buttonData.buttons1 = buttons1_state;
            pico_set_led(buttons1_state);
        }
        
        if (buttons2_state != b2_prev_state) {
            buttonData.buttons2 = buttons2_state;
            pico_set_led(buttons2_state);
        }

        // Guitar special
        if (!(received_data[0] & BUTTON_SPEC_ADDR)) {
            buttonData.ry = 20000;
        } else {
            buttonData.ry = -20000;
        }

        if (tud_suspended())
            tud_remote_wakeup();

        // Send via USB if it is connected, else send to receiver
        if (tud_ready())
        {
            sendReportData(&buttonData);
        }
        else
        {
            if (current_state == PAIRED)
            {
                send_data(radio, buttonData);
            }
            else if (current_state == UNPAIRED && (board_millis() - timeSinceLastAttempt) > RETRY_DELAY)
            {
                pair(radio);
            }

            if (tries >= 3)
            {
                printf("Transmitter: Max tries exceded, closing program\n");
                break;
            }

            // Unpair from current receiver
            // if (current_state == PAIRED && !gpio_get(BUTTON_SYNC_PIN))
            // {
            //     current_state = UNPAIRED;
            //     radio.openWritingPipe(PAIR_ADDR);
            //     unpairedTimer = board_millis();
            // }

            // Stop loop to turn off controller
            // if (current_state == UNPAIRED && (board_millis() - unpairedTimer) >= SYNC_TIMEOUT) {
            //     break;
            // }
        }

        b1_prev_state = buttons1_state;
        b2_prev_state = buttons2_state;
    }
}

void send_data(RF24 radio, XInputReport buttonData)
{
    if (!flDataSent)
    {
        flDataSent = true;
        printf("Transmitter: Data sent!\n");
    }

    DataPacket data_to_send;
    data_to_send.buttons1 = buttonData.buttons1;
    data_to_send.buttons2 = buttonData.buttons2;
    data_to_send.lt = buttonData.lt;
    data_to_send.rt = buttonData.rt;
    data_to_send.lx = buttonData.lx;
    data_to_send.ly = buttonData.ly;
    data_to_send.rx = buttonData.rx;
    data_to_send.ry = buttonData.ry;

    radio.setPayloadSize(sizeof(DataPacket));
    radio.write(&data_to_send, sizeof(data_to_send));

    delay(10); // Small delay between data sends
}

void pair(RF24 radio)
{
    loop_blink(1, 100);

    RequestPacket req;
    if (my_id)
    {
        req.tx_id = my_id;
    }
    req.type = REQUEST_PAIR;

    printf("Transmitter: Sending pair msg\n");
    radio.openWritingPipe(PAIR_ADDR);
    radio.setPayloadSize(sizeof(RequestPacket));
    bool report = radio.write(&req, sizeof(req));

    if (report)
    {
        if (radio.available())
        {
            PairPacket pairPacket;
            radio.read(&pairPacket, sizeof(pairPacket));

            if (pairPacket.available)
            {
                uint8_t conf_addr[6];
                memcpy(conf_addr, pairPacket.conf_addr, 5);
                conf_addr[5] = 0; // Make it readable string
                printf("Transmitter: Conf: %s\n", conf_addr);
                confirm(radio, conf_addr);
            }
            else
            {
                printf("Transmitter: Pairing denied by receiver\n");
                tries++;
            }
        }
        else
        {
            printf("Transmitter: Pairing ACK received, but no grant payload\n");
            tries++;
        }
    }
    else
    {
        printf("Transmitter: Pairing request failed (no ACK)\n");
        tries++;
    }
    timeSinceLastAttempt = board_millis();
}

void confirm(RF24 radio, uint8_t conf_addr[6])
{
    printf("Transmitter: Sending confirm msg\n");
    radio.openWritingPipe(conf_addr);

    RequestPacket req;
    if (my_id)
    {
        req.tx_id = my_id;
    }
    req.type = REQUEST_CONFIRM;

    radio.setPayloadSize(sizeof(ConfirmPacket));
    bool report = radio.write(&req, sizeof(req));

    if (report)
    {
        if (radio.available())
        {
            ConfirmPacket confirmPacket;
            radio.read(&confirmPacket, sizeof(confirmPacket));

            if (my_id && my_id != confirmPacket.tx_id)
            {
                printf("Transmitter: Confirm denied, invalid id\n");
            }
            else if ((my_id && my_id == confirmPacket.tx_id) || !my_id)
            {
                memcpy(coms_addr, confirmPacket.comm_addr, 5);
                coms_addr[5] = 0; // Make it readable string
                printf("Transmitter: Pairing granted! Using address: %s\n", coms_addr);
                radio.openWritingPipe(coms_addr);

                current_state = PAIRED;
                loop_blink(4, 80);
                // save_new_id(confirmPacket);
            }
        }
        else
        {
            printf("Transmitter: Confirm ACK received, but no payload\n");
            tries++;
        }
    }
    else
    {
        printf("Transmitter: Confirm request failed (no ACK)\n");
        tries++;
    }
}

void save_new_id(ConfirmPacket confirmPacket)
{
    if (confirmPacket.tx_id)
    {
        my_id = confirmPacket.tx_id;
        // TODO: Save new id to storage
    }
}
