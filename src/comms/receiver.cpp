#include "comms.h"

const uint8_t PAIR_ADDR[6] = "PAIR0";
const uint8_t COMM_ADDR[6] = "COMM1"; // Common address for initial contact
const unsigned long PAIRING_TIMEOUT = 5000;

void write_pair_ack(RF24 radio);
void write_data_ack(RF24 radio, uint8_t pipe);
void pair(RF24 radio, uint8_t pipe_num);
void receive_data(RF24 radio, XInputReport buttonData, bool *prev_state);

uint32_t lastPacketTimestamp = 0;
bool is_paired = false;
uint16_t paired_tx_id = 0;

static inline uint32_t board_millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void setup_receiver(RF24 radio)
{
    lastPacketTimestamp = board_millis();

    // Initially, listen for pairing requests on Pipe 0
    radio.openReadingPipe(0, PAIR_ADDR);

    write_pair_ack(radio);

    radio.startListening();
    printf("Receiver: Listening for pairing requests...\n");
}

void loop_receiver(RF24 radio, XInputReport buttonData)
{
    bool prev_state = 0;

    while (true)
    {
        if(!is_paired) {
            loop_blink(1, 100);
        }

        // --- Check for incoming packets ---
        uint8_t pipe_num;
        if (radio.available(&pipe_num))
        {
            printf("Pipe: %i\n", pipe_num);
            uint8_t len = radio.getDynamicPayloadSize();
            if (len == 0)
                break; // Should not happen with dynamic payloads if available is true

            if (pipe_num == 0 && !is_paired)
            {
                pair(radio, pipe_num);
                write_pair_ack(radio);
            }
            else if (pipe_num != 0 && is_paired)
            {
                receive_data(radio, buttonData, &prev_state);
            }
            else
            {
                radio.flush_rx();
                printf("Receiver: Unexpected packet flushed, pipe: %i, paired: %i\n", pipe_num, is_paired);
            }
        }

        if (!gpio_get(BUTTON_RB_PIN))
        {
            is_paired = false;
            radio.closeReadingPipe(1);
            tud_disconnect();
            write_pair_ack(radio);
            printf("Pairing...\n");
        }
    }
}

void write_pair_ack(RF24 radio)
{
    GrantPacket grant_packet;
    grant_packet.granted = true;
    grant_packet.tx_id = (uint16_t)rand();
    memcpy(grant_packet.comm_address, COMM_ADDR, 5);
    radio.writeAckPayload(0, &grant_packet, sizeof(grant_packet));
}

void pair(RF24 radio, uint8_t pipe_num)
{
    RequestPacket req_packet;
    radio.read(&req_packet, sizeof(req_packet)); // Adjust size if not using dynamic

    if (req_packet.type == REQUEST_PAIR)
    {
        printf("Receiver: Pairing request received\n");
        
        // Switch to paired state
        is_paired = true;
        paired_tx_id = req_packet.tx_id;
        lastPacketTimestamp = board_millis(); // Reset timeout timer

        radio.openReadingPipe(1, COMM_ADDR);
        
        tud_connect();
        loop_blink(4, 80);
        printf("Receiver: Paired with TX %i on address: %i\n", paired_tx_id, COMM_ADDR);
    }
    else
    {
        printf("Data: %i\n", req_packet.type);
    }
}

void receive_data(RF24 radio, XInputReport buttonData, bool *prev_state)
{
    DataPacket data_packet;
    radio.read(&data_packet, sizeof(data_packet));

    if (data_packet.type == DATA)
    {
        tud_task();

        if (data_packet.buttons2 != *prev_state)
        {
            printf("State: %i\n", data_packet.buttons2);
            pico_set_led(data_packet.buttons2);
            buttonData.buttons2 = data_packet.buttons2;
        }

        sendReportData(&buttonData);
        *prev_state = data_packet.buttons2;
        lastPacketTimestamp = board_millis();
    }
}
