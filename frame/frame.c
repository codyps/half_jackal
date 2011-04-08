#include <stdbool.h>
#include <stdint.h>

#include <util/crc.h>

#include <frame/hw_proto.h>

#if 0
typedef struct frame_send_ctx {
	uint16_t crc;
	void (*putchar)(uint8_t c);
} frame_send_ctx;

void frame_send_start(frame_send_ctx *fc)
{
	fc->putchar(FRAME_START);
	fc->crc = FRAME_CRC_INIT;
}

static void _frame_send_byte(frame_send_ctx *fc, uint8_t c)
{
	if (FRAME_NEED_ESC(c)) {
		fc->putchar(FRAME_ESC);
		c ^= FRAME_ESC_MASK;
	}

	fc->putchar(c);
}

void frame_send_byte(frame_send_ctx *fc, uint8_t c)
{
	fc->crc = _crc_ccitt_update(fc->crc, c);
	_frame_send_byte(fc, c);
}

void frame_send_data(frame_send_ctx *fc, void *data, uint8_t len)
{
	uint8_t *d = data;
	for (;;) {
		if (len == 0)
			break;
		frame_send_byte(*d);
		d++;
		len--;
	}
}

void frame_send_done(frame_send_ctx *fc)
{
	uint16_t crc = fc->crc;

	/* crc must be sent msb first */
	_frame_send_byte(fc, crc >> 8);
	_frame_send_byte(fc, crc & 0xff);

	fc->putchar(FRAME_START);
}

typedef struct frame_recv_ctx {
	uint8_t head;
	uint8_t tail;
	uint8_t data[64];

	uint16_t crc;

	bool started;
	bool esc;
	bool x;
} frame_recv_ctx;

void frame_recv_feed(frame_recv_ctx *fc, uint8_t c)
{
	uint8_t head = fc->head;
	uint8_t phead = fc->data[head];

	/* FIXME: is this +1 needed? */
	uint8_t next_pos = (head + phead + 1) & (sizeof(fc->data) - 1);

	if (c == FRAME_START) {
		fc->started = true;
		fc->esc = false;

		if (phead != 0) {
			/* current packet has data */

			if (fc->crc != 0 || next_pos == fc->tail) {
				/* Invalid CRC or no space. Avoid packet
				 * advance */
				phead = 0;
				fc->data[head] = phead;
			} else {
				/* trim crc */
				phead -= FRAME_CRC_SZ;
				fc->data[head] = phead;

				/* FIXME: should this be next_pos - 1? */
				fc->head = next_pos;

				/* packet has 0 len initially */
				fc->data[fc->head] = 0;
			}

		}

		fc->crc = FRAME_CRC_INIT;
		return;
	}


	if (!fc->started) {
		return;
	}

	if (c == FRAME_RESET) {
		goto drop_packet;
	}

	if (c == FRAME_ESC) {
		fc->esc = true;
		return;
	}

	if (fc->esc) {
		fc->esc = false;
		c ^= FRAME_ESC_MASK;
	}

	fc->crc = _crc_ccitt_update(fc->crc, c);

	if (next_pos != fc->tail) {
		/* there is more space */
		uint8_t pos = (head + phead) & (sizeof(fc->data) - 1);
		fc->data[pos] = c;
		phead += 1;
		fc->data[head] = phead;
	}

	/* no more space */

drop_packet:
	fc->started = false;
	fc->esc = false;
	fc->crc = FRAME_CRC_INIT;

	fc->data[head] = 0;
}

void frame_recv_error(frame_recv_ctx *fc)
{
	/* drop current packet. */
	fc->started = false;
	fc->esc = false;
	fc->crc = FRAME_CRC_INIT;
	fc->data[fc->head] = 0;
}

/* returns len */
uint8_t frame_recv_start(frame_recv_ctx *fc)
{
	/* no packet */
	if (fc->tail == fc->head)
		return 0;

	return fc->data[fc->tail];
}

uint8_t frame_recv_u8(frame_recv_ctx *fc)
{
	/* return the next byte from the current packet and advance the tail */
	/* ASSUMED: there exsists data in the current packet */

	/* fc->tail always points to the packet len */
	uint8_t nlen = fc->data[fc->tail] - 1;
	uint8_t ntail = (fc->tail + 1) & (sizeof(fc->data) - 1);
	uint8_t d = fc->data[ntail];

	/* XXX: ordering appears unimportant, check? */
	fc->tail = ntail;
	fc->data[ntail] = nlen;

	return d;
}

void frame_recv_done(frame_recv_ctx *fc)
{
	/* TODO: update tail to the start of the next packet */
	fc->tail = (fc->tail + fc->data[fc->tail]) & (sizeof(fc->data) - 1);
}

void frame_recv_copy(frame_recv_ctx *fc, uint8_t *dst, uint8_t len)
{
	
}
#endif
