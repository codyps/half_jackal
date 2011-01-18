#ifndef CRC_H_
#define CRC_H_

static inline
uint16_t crc_ccitt_update(uint16_t crc, uint8_t data)
{
	data ^= lo8(crc);
	data ^= data << 4;

	return ((((uint16_t) data << 8) | hi8(crc)) ^ (uint8_t) (data >> 4)
		^ ((uint16_t) data << 3));
}

#endif
