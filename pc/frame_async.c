#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include "crc.h"
#include "../frame_proto.h"

#define SEND_BYTE(out, c) do {						\
	if ((c) == START_BYTE || (c) == ESC_BYTE || (c) == RESET_BYTE) {\
		fputc(ESC_BYTE, (out));					\
		fputc(ESC_MASK ^ (c), (out));				\
	} else {							\
		fputc((c), (out));					\
	}								\
} while(0)

ssize_t frame_send(FILE *out, void *data, size_t nbytes)
{
	char *d;
	char *end;
	uint16_t crc = CRC_INIT;

	fputc(START_BYTE, out);
	for(d = data, end = d + nbytes; d < end; d++) {
		char c = *d;
		crc = crc_ccitt_update(crc, c);
		SEND_BYTE(out, c);
	}

	SEND_BYTE(out, crc & 0xff);
	SEND_BYTE(out, crc >> 8);

	fputc(START_BYTE, out);

	fflush(out);
	return nbytes;
}

ssize_t frame_recv(FILE *in, void *vbuf, size_t nbytes)
{
	uint16_t crc = CRC_INIT;
	size_t i;
	char *buf = vbuf;
	bool recv_started = false;
	bool is_escaped = false;

	for(i = 0;;) {
		int data = fgetc(in);

		if (data == EOF)
			break;

		if (data < 0) {
			return data;
		}

		if (ferror(in)) {
			return -255;
		}

		if (data == START_BYTE) {
			if (recv_started) {
				if (i != 0) {
					/* success */
					if (crc == 0) {
						ungetc(data, in);
						return i;
					} else {
						fprintf(stderr, "crc = %d\n", crc);
						i = 0;
						crc = CRC_INIT;
						continue;
					}
				}
			} else {
				recv_started = true;
			}
			continue;
		}

		if (!recv_started)
			continue;

		if (data == RESET_BYTE) {
			/* restart recv */
			i = 0;
			continue;
		}

		if (data == ESC_BYTE) {
			is_escaped = true;
			continue;
		}

		if (is_escaped) {
			is_escaped = false;
			data ^= ESC_MASK;
		}

		crc = crc_ccitt_update(crc, (uint8_t)(data & 0xff));

		if (i < nbytes) {
			buf[i] = data;
			i++;
		} else {
			return -ENOSPC;
		}
	}
	return i;
}
