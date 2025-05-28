#include "comms.h"

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
    radio.setRetries(5, 15);
    printf("Transmitter: Starting transmitter module\n");
}

void loop_transmitter(RF24 radio, XInputReport buttonData)
{
    bool prev_state = 0;

    while (true)
    {
        // Update USB stack
        tud_task();

        // Mount data to be sent
        uint8_t state = bools_to_uint8(
            0, !gpio_get(BUTTON_LB_PIN),
            0, 0,
            !gpio_get(BUTTON_A_PIN), !gpio_get(BUTTON_B_PIN),
            !gpio_get(BUTTON_X_PIN), !gpio_get(BUTTON_Y_PIN));

        if (state != prev_state)
        {
            buttonData.buttons2 = state;
            printf("Transmitter: Button changed\n");
            pico_set_led(state);
        }

        // if (tud_suspended())
        //     tud_remote_wakeup();

        // Send via USB if it is connected, else send to receiver
        // if (tud_ready())
        // {
        //     sendReportData(&buttonData);
        // }
        // else
        // {
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
        // if (current_state == PAIRED && !gpio_get(BUTTON_RB_PIN))
        // {
        //     current_state = UNPAIRED;
        //     radio.openWritingPipe(PAIR_ADDR);
        //     unpairedTimer = board_millis();
        // }

        // Stop loop to turn off controller
        // if (current_state == UNPAIRED && (board_millis() - unpairedTimer) >= SYNC_TIMEOUT) {
        //     break;
        // }

        prev_state = state;
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
    data_to_send.buttons2 = buttonData.buttons2;

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
