#ifndef FRAME_PROTO_H_
#define FRAME_PROTO_H_

#include <stdint.h>

/* CRC = ccitt
 * CRC is preformed over actual data instead of
 * escaped data.
 */




#define FRAME_CRC_INIT 0xffff
#define FRAME_CRC_SZ sizeof(uint16_t)

#define FRAME_START ((uint8_t)0x7e)
#define FRAME_RESET ((uint8_t)0x7f)
#define FRAME_ESC   ((uint8_t)0x7d)
#define FRAME_ESC_MASK   ((uint8_t)0x20)

#define FRAME_ESC_CHECK(c) \
	((c) == FRAME_START || (c) == FRAME_RESET || (c) == FRAME_ESC)

#endif
