#include "comms.h"

enum TxState
{
    UNPAIRED,
    WAITING_FOR_ACK,
    PAIRED
};
TxState current_state = UNPAIRED;

const uint8_t PAIR_ADDR[6] = "PAIR0";
const unsigned long RETRY_DELAY = 500; // ms
const unsigned long TIMEOUT_DELAY = 60000; // 1min

uint16_t my_id = 0;
uint32_t timeSinceLastAttempt = 0;
uint32_t timeSinceUnpaired = 0;
bool isFirstExecution = true; 

uint8_t comm_addr[6];

void pair(RF24 radio);
void send_data(RF24 radio, XInputReport buttonData);
void save_new_id(GrantPacket grantAck);


static inline uint32_t board_millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void setup_transmitter(RF24 radio)
{
    // Get last saved id
    // my_id = get_saved_id();
    
    radio.setRetries(5, 15);
    radio.openWritingPipe(PAIR_ADDR);
}

void loop_transmitter(RF24 radio, XInputReport buttonData)
{
    timeSinceUnpaired = board_millis();
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

        // Unpair from current receiver
        if (current_state == PAIRED && !gpio_get(BUTTON_RB_PIN))
        {
            current_state = UNPAIRED;
            radio.openWritingPipe(PAIR_ADDR);
            timeSinceUnpaired = board_millis();
        }

        // Stop loop to turn off controller
        // if (current_state == UNPAIRED && (board_millis() - timeSinceUnpaired) > RETRY_DELAY) {
        //     break;
        // }
        // }

        prev_state = state;

    }
}

void send_data(RF24 radio, XInputReport buttonData)
{
    DataPacket data_to_send;
    data_to_send.type = DATA;
    data_to_send.buttons2 = buttonData.buttons2;

    radio.setPayloadSize(sizeof(DataPacket));
    radio.write(&data_to_send, sizeof(data_to_send));

    delay(10); // Small delay between data sends
}

void pair(RF24 radio)
{
    loop_blink(1, 100);

    RequestPacket req;
    if (my_id != 0) {
        req.tx_id = my_id;
    }

    radio.setPayloadSize(sizeof(RequestPacket));
    bool report = radio.write(&req, sizeof(req));
    
    if (report)
    {
        if (radio.available())
        {
            GrantPacket grantAck;
            radio.read(&grantAck, sizeof(grantAck));

            if (grantAck.granted)
            {
                memcpy(comm_addr, grantAck.comm_address, 5);
                comm_addr[5] = 0; // Make it readable string
                printf("Transmitter: Pairing granted! Using address: %i\n", comm_addr);

                loop_blink(4, 80);

                current_state = PAIRED;
                radio.openWritingPipe(comm_addr); // Use the new address
                save_new_id(grantAck);
            }
            else
            {
                printf("Transmitter: Pairing denied by receiver\n");
                timeSinceLastAttempt = board_millis();
            }
        }
        else
        {
            printf("Transmitter: ACK received, but no grant payload (or RX busy)\n");
            timeSinceLastAttempt = board_millis();
        }
    }
    else
    {
        printf("Transmitter: Pairing request failed (no ACK)\n");
        timeSinceLastAttempt = board_millis();
    }
}

void save_new_id(GrantPacket grantAck)
{
    if (grantAck.tx_id)
    {
        my_id = grantAck.tx_id;
        // TODO: Save new id to storage
    }
}
