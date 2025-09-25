#include "receiver.h"

// Methods
void setup_receiver(RF24 radio);
void populate_addrs();
void write_pair_ack(RF24 radio);
void loop_receiver(RF24 radio, XInputReport buttonData);
void pair(RF24 radio, uint8_t pipeNum);
void write_confirm_ack(RF24 radio, uint16_t req_id);
void receive_data(RF24 radio, XInputReport buttonData, DataPacket *prevState);
void update_button_data(XInputReport *buttonData, DataPacket dataPacket);

// Variables
uint32_t lastPacketTimestamp = 0;
uint32_t syncTimer = 0;
uint16_t paired_tx_id = 0;
uint16_t pairedIds[16];
int retries = 0;
bool isPaired = false;
bool isSyncing = false;

uint8_t conf_addr[6];
uint8_t comm_addr[6];

int loadedPayloads = 0;
bool flBufferFull = false;

static inline uint32_t board_millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void setup_receiver(RF24 radio)
{
    lastPacketTimestamp = board_millis();

    populate_addrs();

    // Open pair and confirm pipes
    radio.openReadingPipe(PAIR_PIPE, PAIR_ADDR);
    radio.openReadingPipe(CONF_PIPE, conf_addr);

    // Clear and write initial pair ack payload
    printf("Receiver: FIFO state: %i\n", radio.isFifo(true));
    write_pair_ack(radio);

    radio.startListening();
    pico_set_led(true);
    printf("Receiver: Listening for pairing requests...\n");
}

void populate_addrs()
{
    generate_random_addr(conf_addr);
    memcpy(comm_addr, conf_addr, 6);
    comm_addr[0] = 66;
    printf("Receiver: Comm: %s, Conf: %s\n", conf_addr, comm_addr);
}

void write_pair_ack(RF24 radio)
{
    // Create ack payload with availability info
    PairPacket pairPacket;
    pairPacket.available = !isPaired;
    memcpy(pairPacket.conf_addr, conf_addr, 5);
    bool res = radio.writeAckPayload(PAIR_PIPE, &pairPacket, sizeof(pairPacket));
    loadedPayloads++;

    if (res)
    {
        printf("Receiver: Pairing ack loaded\n");
    }
    else
    {
        printf("Receiver: Pairing ack not loaded\n");
        retries++;
    }
}

void loop_receiver(RF24 radio, XInputReport buttonData)
{
    DataPacket prevState;

    while (true)
    {
        // Blink while looking for new devices
        if (isSyncing)
        {
            loop_blink(1, 100);
        }

        // Unpair if no packet is received after timeout
        if (isPaired && (board_millis() - lastPacketTimestamp) >= PAIRING_TIMEOUT)
        {
            isPaired = false;
            radio.closeReadingPipe(COMM_PIPE);
            printf("Receiver: Unpaired due to timeout\n");
        }

        // Check for incoming packets
        uint8_t pipeNum;
        if (radio.available(&pipeNum))
        {
            uint8_t len = radio.getDynamicPayloadSize();

            if (pipeNum == PAIR_PIPE || pipeNum == CONF_PIPE)
            {
                pair(radio, pipeNum);
            }
            else if (pipeNum != PAIR_PIPE && pipeNum != CONF_PIPE && isPaired)
            {
                receive_data(radio, buttonData, &prevState);
            }
            else
            {
                printf("Receiver: Unexpected packet flushed, pipe: %i, paired: %i\n", pipeNum, isPaired);
                radio.flush_rx();
            }
        }

        if (retries >= 3)
        {
            radio.flush_tx();

            loadedPayloads = 0;
            flBufferFull = false;
            retries = 0;
            printf("Receiver: Flushing TX buffer\n");
        }

        if (loadedPayloads >= 3 && !flBufferFull)
        {
            flBufferFull = true;
            printf("Receiver: Payload buffer full\n");
        }

        // Sync button pressed
        if (!gpio_get(BUTTON_RB_PIN))
        {
            isSyncing = true;
            syncTimer = board_millis();
            printf("Receiver: Pairing...\n");
        }

        if (isSyncing && (board_millis() - syncTimer) >= SYNC_TIMEOUT)
        {
            isSyncing = false;
            printf("Receiver: Finished Pairing...\n");
        }
    }
}

void pair(RF24 radio, uint8_t pipeNum)
{
    RequestPacket reqPacket;
    radio.read(&reqPacket, sizeof(reqPacket));

    if (pipeNum == PAIR_PIPE)
    {
        // TODO: Understand why sync is unstable. Generally it works right after turning on, but subsequent tend to fail
        // I think it can be related to the TX FIFO buffer. There could be a way to check what's inside the buffer
        // At any given time, at least one pair ack should be written. The other two could be confirm, given that after sent, a pair exists 

        printf("Receiver: Pairing request received\n");
        printf("Receiver: Pre-pair FIFO state: %i\n", radio.isFifo(true));
        write_confirm_ack(radio, reqPacket.tx_id);
        write_pair_ack(radio);
        printf("Receiver: Post-pair FIFO state: %i\n", radio.isFifo(true));
    }
    else
    {
        printf("Receiver: Confirm request received\n");

        if (isSyncing)
        {
            isSyncing = false;
            printf("Receiver: Finished Pairing...\n");
        }

        radio.flush_tx();
        write_pair_ack(radio);

        radio.openReadingPipe(COMM_PIPE, comm_addr);
        tud_connect();
        loop_blink(4, 80);
        pico_set_led(true);
        isPaired = true;

        printf("Receiver: Paired with TX %i on address: %s\n", reqPacket.tx_id, comm_addr);
        lastPacketTimestamp = board_millis(); // Reset timeout timer
    }
}

void write_confirm_ack(RF24 radio, uint16_t req_id)
{
    ConfirmPacket confirmPacket;
    if (req_id)
    {
        confirmPacket.tx_id = req_id;
    }
    else if (!req_id && isSyncing)
    {
        // A new id can only be created while in sync mode
        confirmPacket.tx_id = (uint16_t)get_rand_32();
    }

    memcpy(confirmPacket.comm_addr, comm_addr, 5);
    bool res = radio.writeAckPayload(CONF_PIPE, &confirmPacket, sizeof(confirmPacket));

    // TODO: Remove debug lines
    loadedPayloads++;

    if (res)
    {
        printf("Receiver: Confirm ack loaded\n");
    }
    else
    {
        printf("Receiver: Confirm ack not loaded\n");
        retries++;
    }
}

void receive_data(RF24 radio, XInputReport buttonData, DataPacket *prevState)
{
    DataPacket data_packet;
    radio.read(&data_packet, sizeof(data_packet));

    tud_task();

    if (data_packet.operator==(*prevState))
    {
        pico_set_led(data_packet.buttons2);
        update_button_data(&buttonData, data_packet);
    }

    if (tud_ready()) {
        sendReportData(&buttonData);
    }
    *prevState = data_packet;
    lastPacketTimestamp = board_millis();
}

void update_button_data(XInputReport *buttonData, DataPacket dataPacket)
{
    buttonData->buttons1 = dataPacket.buttons1;
    buttonData->buttons2 = dataPacket.buttons2;
    buttonData->lt = dataPacket.lt;
    buttonData->lx = dataPacket.lx;
    buttonData->lt = dataPacket.ly;
    buttonData->rt = dataPacket.rt;
    buttonData->rx = dataPacket.rx;
    buttonData->ry = dataPacket.ry;
}