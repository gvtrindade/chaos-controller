#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_PORT i2c0

// IO Expander addresses
const uint8_t CHIP_ADDRS[] = {0x20, 0x21};
const int NUM_CHIPS = sizeof(CHIP_ADDRS) / sizeof(CHIP_ADDRS[0]);

// Chip 0x20
const uint8_t BUTTON_UP_ADDR = 0x80;     // P7
const uint8_t BUTTON_DOWN_ADDR = 0x40;   // P6
const uint8_t BUTTON_LEFT_ADDR = 0x20;   // P5
const uint8_t BUTTON_RIGHT_ADDR = 0x10;  // P4
const uint8_t BUTTON_START_ADDR = 0x08;  // P3
const uint8_t BUTTON_SELEC_ADDR = 0x04;  // P2
const uint8_t BUTTON_XCE_ADDR = 0x02;    // P1
const uint8_t BUTTON_SPEC_ADDR = 0x01;   // P0

// Chip 0x21
const uint8_t BUTTON_GRE_ADDR = 0x80;   // P7
const uint8_t BUTTON_RED_ADDR = 0x40;   // P6
const uint8_t BUTTON_YEL_ADDR = 0x20;   // P5
const uint8_t BUTTON_BLU_ADDR = 0x10;   // P4
const uint8_t BUTTON_ORA_ADDR = 0x08;   // P3
const uint8_t BUTTON_STRU_ADDR = 0x04;  // P2
const uint8_t BUTTON_STRD_ADDR = 0x02;  // P1