#include <RF24.h>

#include "tusb.h"
#include "utils.h"
#include "XInputPad.h"
#include "pico/stdlib.h"

// Contants
const int PAIR_PIPE = 0;
const int CONF_PIPE = 1;
const int COMM_PIPE = 2;
const uint8_t PAIR_ADDR[6] = "Prime";
const unsigned long PAIRING_TIMEOUT = 5000;
const unsigned long SYNC_TIMEOUT = 60000; // 1min

// Structs
enum PacketType: uint8_t {
    REQUEST_PAIR = 1,
    REQUEST_CONFIRM = 2,
    DATA = 3, 
};

struct RequestPacket {
    PacketType type;
    uint16_t tx_id;
};

struct PairPacket
{
    bool available = false;
    uint8_t conf_addr[5];
};

struct ConfirmPacket
{
    uint16_t tx_id;
    uint8_t comm_addr[5];
};

struct DataPacket
{
    uint8_t buttons1;
    uint8_t buttons2;
    uint8_t lt;
    uint8_t rt;
    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;

    bool operator==(const DataPacket &other) const {
    return this->buttons1 == other.buttons1
        && this->buttons2 == other.buttons2
        && this->lt == other.lt
        && this->rt == other.rt
        && this->lx == other.lx
        && this->ly == other.ly
        && this->rx == other.rx
        && this->ry == other.ry;
  }
};

void setup_transmitter(RF24 radio);
void loop_transmitter(RF24 radio);
