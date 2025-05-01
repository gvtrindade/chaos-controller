#include "comms.h"

const uint8_t PAIRING_ADDR[6] = "PAIR0"; // Common address for initial contact
uint8_t current_comm_addr[6] = "COMM1";  // Placeholder, will be generated/assigned

uint32_t lastPacketTimestamp = 0; // Timer for connection timeout

bool is_paired = false;
uint16_t paired_tx_id = 0;
const unsigned long PAIRING_TIMEOUT = 5000; // 5 seconds timeout

enum TxState
{
    UNPAIRED,
    WAITING_FOR_ACK,
    PAIRED
};
TxState current_state = UNPAIRED;

uint8_t comm_addr[6];       // Address receiver told us to use
uint16_t my_tx_id = 0xABCD; // Assign a unique ID to each transmitter
uint32_t timeSinceLastAttempt = 0;
const unsigned long RETRY_DELAY = 500; // Wait 500ms between pairing attempts

static inline uint32_t board_millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void setup_receiver(RF24 radio)
{
    lastPacketTimestamp = board_millis();

    // Initially, listen for pairing requests on Pipe 0
    radio.openReadingPipe(1, PAIRING_ADDR);

    GrantPacket grant_packet;
    grant_packet.granted = true;
    memcpy(grant_packet.comm_address, current_comm_addr, 5);
    radio.writeAckPayload(1, &grant_packet, sizeof(grant_packet));

    radio.startListening();
    printf("Receiver: Listening for pairing requests...\n");
}

void handle_timeout(RF24 radio)
{
    printf("Receiver: Timeout from TX %i\n", paired_tx_id);
    is_paired = false;
    paired_tx_id = 0;

    tud_disconnect();

    radio.stopListening();
    radio.closeReadingPipe(1);              // Close the dedicated communication pipe
    radio.openReadingPipe(1, PAIRING_ADDR); // Re-open pairing pipe

    GrantPacket grant_packet;
    grant_packet.granted = true;
    memcpy(grant_packet.comm_address, current_comm_addr, 5);
    radio.writeAckPayload(1, &grant_packet, sizeof(grant_packet));

    radio.startListening();
    printf("Receiver: Listening for pairing requests again\n");
}

void handle_receiver_pairing(RF24 radio, uint8_t pipe_num)
{
    RequestPacket req_packet;
    radio.read(&req_packet, sizeof(req_packet)); // Adjust size if not using dynamic

    if (req_packet.type == REQUEST_PAIR)
    {
        printf("Receiver: Pairing request received from TX %i\n", req_packet.tx_id);

        // Generate or assign a unique communication address
        // Example: derive from base "COMM" + TX ID or use a counter
        // For simplicity, let's just use a fixed one for Pipe 1 for now
        // A real implementation should ensure uniqueness if multiple TXs exist
        memcpy(current_comm_addr, "COMM1", 5); // Assign a unique address

        // Switch to paired state
        is_paired = true;
        paired_tx_id = req_packet.tx_id;
        lastPacketTimestamp = board_millis(); // Reset timeout timer

        // Switch listening pipes
        radio.stopListening();
        radio.closeReadingPipe(1);                   // Close pairing pipe
        radio.openReadingPipe(1, current_comm_addr); // Open dedicated pipe
        radio.startListening();
        tud_connect();
        printf("Receiver: Paired with TX %i on address: %i\n", paired_tx_id, current_comm_addr);
    }
    else
    {
        printf("Data: %i\n", req_packet.type);
    }
}

void handle_receive_data(RF24 radio, XInputReport buttonData, bool *prev_state)
{
    DataPacket data_packet;
    radio.read(&data_packet, sizeof(data_packet));

    if (data_packet.type == DATA)
    {
        tud_task();

        printf("State: %i\n", data_packet.buttons2);
        if (data_packet.buttons2 != *prev_state)
        {
            pico_set_led(data_packet.buttons2);
            buttonData.buttons2 = data_packet.buttons2;
        }

        sendReportData(&buttonData);
        *prev_state = data_packet.buttons2;
        lastPacketTimestamp = board_millis();
    }
    else if (data_packet.type == DISCONNECT)
    {
        printf("Receiver: Disconnect received from TX %i\n", paired_tx_id);
        lastPacketTimestamp = board_millis() - PAIRING_TIMEOUT - 1;
        tud_disconnect();
    }
}

void loop_receiver(RF24 radio, XInputReport buttonData)
{
    bool prev_state = 0;

    while (true)
    {
        // --- Timeout Handling ---
        if (is_paired && (board_millis() - lastPacketTimestamp) > PAIRING_TIMEOUT)
        {
            handle_timeout(radio);
        }

        // --- Check for incoming packets ---
        uint8_t pipe_num;
        if (radio.available(&pipe_num))
        {
            uint8_t len = radio.getDynamicPayloadSize();
            if (len == 0)
                break; // Should not happen with dynamic payloads if available is true

            if (pipe_num == 1 && !is_paired)
            {
                handle_receiver_pairing(radio, pipe_num);
            }
            else if (pipe_num == 1 && is_paired)
            {
                handle_receive_data(radio, buttonData, &prev_state);
            }
            else
            {
                radio.flush_rx();
                printf("Receiver: Unexpected packet flushed\n");
            }
        }
    }
}

void setup_transmitter(RF24 radio)
{
    radio.setRetries(5, 15);
}

void handle_transmitter_pairing(RF24 radio)
{
    radio.stopListening();               
    radio.openWritingPipe(PAIRING_ADDR);

    RequestPacket req;
    req.tx_id = my_tx_id;

    radio.setPayloadSize(sizeof(RequestPacket));
    bool report = radio.write(&req, sizeof(req));

    if (report)
    {
        uint8_t pipe;
        if (radio.available(&pipe))
        {
            GrantPacket grant_ack;
            radio.read(&grant_ack, sizeof(grant_ack));
            if (grant_ack.granted)
            {
                memcpy(comm_addr, grant_ack.comm_address, 5);
                comm_addr[5] = 0; // Null terminate if needed for printing etc.
                printf("Transmitter: Pairing granted! Using address: %i\n", comm_addr);

                current_state = PAIRED;
                radio.stopListening();
                radio.openWritingPipe(comm_addr); // Use the new address
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

void handle_send_data(RF24 radio, XInputReport buttonData)
{
    DataPacket data_to_send;
    data_to_send.type = DATA;
    data_to_send.buttons2 = buttonData.buttons2;

    radio.setPayloadSize(sizeof(DataPacket));
    bool report = radio.write(&data_to_send, sizeof(data_to_send));

    if (!report)
    {
        printf("Transmitter: Data send failed (No ACK from RX). Assuming disconnected\n");
        current_state = UNPAIRED;
        timeSinceLastAttempt = board_millis();
    }

    delay(10); // Small delay between data sends
}

void loop_transmitter(RF24 radio, XInputReport buttonData)
{
    bool prev_state = 0;
    while (true)
    {

        tud_task();

        uint8_t state = bools_to_uint8(
            0, !gpio_get(BUTTON_RB_PIN),
            0, 0,
            !gpio_get(BUTTON_A_PIN), !gpio_get(BUTTON_B_PIN),
            !gpio_get(BUTTON_X_PIN), !gpio_get(BUTTON_Y_PIN));

        if (state != prev_state)
        {
            buttonData.buttons2 = state;
            pico_set_led(state);
        }

        if (tud_suspended())
            tud_remote_wakeup();

        if (tud_ready())
        {
            sendReportData(&buttonData);
        }
        else
        {
            if (current_state == UNPAIRED && (board_millis() - timeSinceLastAttempt) > RETRY_DELAY)
            {
                handle_transmitter_pairing(radio);
            }
            else if (current_state == PAIRED)
            {
                handle_send_data(radio, buttonData);
            }
        }

        prev_state = state;
    }
}
