#include <RF24.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "tusb.h"
#include "XInputPad.h"
#include "utils.h"

enum PacketType: uint8_t {
    REQUEST_PAIR = 1,
    DATA = 2, 
    DISCONNECT = 3
};

struct RequestPacket {
    PacketType type = REQUEST_PAIR;
    uint16_t tx_id;
};

struct GrantPacket {
    bool granted = true;
    uint8_t comm_address[5];
};

struct DataPacket {
    PacketType type = DATA;
    uint8_t buttons2 = 0;
};

void setup_receiver(RF24 radio);
void loop_receiver(RF24 radio, XInputReport buttonData);
void setup_transmitter(RF24 radio);
void loop_transmitter(RF24 radio, XInputReport buttonData);
