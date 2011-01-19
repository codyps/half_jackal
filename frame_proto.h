#ifndef PROTO_H_
#define PROTO_H_

#include <stdint.h>

#define CRC_INIT 0xffff
#define CRC_SZ sizeof(uint16_t)

#define START_BYTE ((uint8_t)0x7e)
#define RESET_BYTE ((uint8_t)0x7f)
#define ESC_BYTE   ((uint8_t)0x7d)
#define ESC_MASK   ((uint8_t)0x20)

#endif
