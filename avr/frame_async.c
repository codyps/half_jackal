#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/crc16.h>

#include "muc.h"
#include "ds/circ_buf.h"

#include "error_led.h"

#include "../frame_proto.h"
/* 0x7f => 0x7d, 0x5f
 * 0x7e => 0x7d, 0x5e
 * 0x7d => 0x7d, 0x5d
 */

/* HDLC
 * [flag] [address] [ctrl] [data] [crc] [flag]
 * addr8 : [multicast:1] [node addr:6] ['1':1]
 * addr16: [multicast:1] [node addr:6] ['0':1] [node addr:7] [end addr:1]...
 * ctrl  :
 */

/* ctrl:
 * Frame 7 6 5 4   3 2 1 0
 * I     N(R)  p   N(S)  0
 * S     N(R)  p/f S S 0 1
 * U     M M M p/f M M 1 1
 */

/* LC:
 *  [flag:8] [addr:8] [len:8] [data1:len] [crc:16] [flag:8] ...
 * data1:
 *  []
 */
#define P_SZ(circ) (sizeof((circ).p_idx))
#define B_SZ(circ) (sizeof((circ).buf))
#define sizeof_member(type, member) \
	sizeof(((type *)0)->member)

struct packet_buf {
	uint8_t buf[32]; /* bytes */

	/* array of packet starts in bytes (byte heads and tails,
	 * depending on index */
	uint8_t p_idx[8];
	uint8_t head; /* next packet_idx_buf loc to read from (head_packet) */
	uint8_t tail; /* next packet_idx_buf loc to write to  (tail_packet) */
};

static struct packet_buf rx, tx;


#if defined(AVR)
/* {{ DEBUG */
# ifdef DEBUG
static int usart0_putchar_direct(char c, FILE *stream) {
	if (c == '\n')
		putc('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static void print_wait(void)
{
	loop_until_bit_is_set(UCSR0A, UDRE0);
}

static FILE usart0_io_direct =
	FDEV_SETUP_STREAM(usart0_putchar_direct, NULL,_FDEV_SETUP_WRITE);
# endif

# define usart0_udre_isr_on() (UCSR0B |= (1 << UDRIE0))
# define usart0_udre_unlock() do {         \
		UCSR0B |= (1 << UDRIE0);  \
		asm("":::"memory");       \
	} while(0)

# define usart0_udre_isr_off() (UCSR0B &= ~(1 << UDRIE0))
# define usart0_udre_lock() do {           \
		UCSR0B &= ~(1 << UDRIE0); \
		asm("":::"memory");       \
	} while(0)

# define RX_BYTE_GET() UDR0
# define RX_STATUS_GET() UCSR0A
# define RX_STATUS_IS_ERROR(status) ((status) &          \
		((1 << FE0) | (1 << DOR0) | (1<< UPE0)))

# define TX_BYTE_SEND(byte) (UDR0 = (byte))

# define RX_ISR() ISR(USART_RX_vect)
# define TX_ISR() ISR(USART_UDRE_vect)

#else /* !defined(AVR) */

# ifdef DEBUG
#  define print_wait()
# endif

# define usart0_udre_isr_on()
# define usart0_udre_unlock()
# define usart0_udre_isr_off()
# define usart0_udre_lock()

# define RX_BYTE_GET() getchar()
# define RX_STATUS_GET() 0
# define RX_STATUS_IS_ERROR(status) false

# define TX_BYTE_SEND(byte) putchar(byte)

# define TX_ISR() void frame_tx_isr(void)
# define RX_ISR() void frame_rx_isr(void)

#endif


#ifdef DEBUG
static void print_packet_buf(struct packet_buf *b)
{
	printf("head %02d  tail %02d  p_idx(%d) ", b->head,
			b->tail, sizeof(b->p_idx));
	uint8_t i;
	for (i = 0; ;) {
		printf("%d", b->p_idx[i]);
		i++;
		if (i < sizeof(b->p_idx))
			putchar(' ');
		else
			break;
	}

	printf("  buf ");
	for(i = 0; i < sizeof(b->buf); i++) {
		printf("%c ", b->buf[i]);
	}
}

void frame_timeout(void)
{
	printf("\n{{ tx: ");
	print_packet_buf(&tx);
	printf(" }}\n{{ rx: ");
	print_packet_buf(&rx);
	printf(" }}\n");

}
#endif /* defined(DEBUG) */

/*** Reception of Data ***/
/** receive: consumer, modifies tail **/
/* 2 paths possible for recviever:
 *  - call frame_recv_len (returns 0 if no packet is ready) to see if there is
 *    a packet, subsequently recv data via frame_recv_byte & frame_recv_copy.
 *  - call frame_recv_copy (returns 0 if no packet) and either have all data
 *    returned in the single copy call, or use subsequent
 *    frame_recv_{copy,byte} calls to get all the data.
 * Once done recieving current packet, call frame_recv_next to advance to the
 * next packet. Packets *are not* advanced automatically under any condition.
 */

/* return: number of bytes in the current packet. 0 indicates the lack of
 * a packet (packets cannot be 0 bytes).
 */
uint8_t frame_recv_len(void)
{
	uint8_t it = rx.tail;
	if (it != rx.head) {
		return CIRC_CNT(rx.p_idx[CIRC_NEXT(it,P_SZ(rx))],
				rx.p_idx[it],
				B_SZ(rx));
	} else {
		return 0;
	}
}

/* return: next byte from packet.
 * advances byte pointer.
 *
 * it is not recommended to call this without first determining that the
 * next byte in the packet exsists. It will return 0 in the case were
 * no bytes remain in the packet, which may be a valid value in the
 * packet and thus should not be used to indicate the packet is
 * fully processed.
 */
uint8_t frame_recv_byte(void)
{
	uint8_t it = rx.tail;
	uint8_t b_it = rx.p_idx[it];
	uint8_t it_1 = CIRC_NEXT(it, P_SZ(rx));
	uint8_t b_it_1 = rx.p_idx[it_1];

	if ((it != rx.head) && (b_it != b_it_1)) {
		uint8_t data = rx.buf[b_it];
		rx.p_idx[it] = CIRC_NEXT(b_it, B_SZ(rx));
		return data;
	} else {
		return 0;
	}
}

/* dst: array of at least len bytes into which the current packet is copied
 *      (up to len bytes).
 * return: length of current packet, not the amount copied into dst.
 * byte pointer is advanced by MIN(len, packet_len)
 */
uint8_t frame_recv_copy(uint8_t *dst, uint8_t len)
{
	uint8_t it = rx.tail;
	if (it != rx.head) {
		uint8_t b_it = rx.p_idx[it];
		uint8_t it_1 = CIRC_NEXT(it, P_SZ(rx));
		uint8_t b_it_1 = rx.p_idx[it_1];
		uint8_t ct = CIRC_CNT(b_it_1, b_it, B_SZ(rx));
		uint8_t ct_to_end = CIRC_CNT_TO_END(b_it_1, b_it, B_SZ(rx));

		uint8_t cpy_ct = MIN(len, ct);
		uint8_t cpy1_len = MIN(len, ct_to_end);
		uint8_t cpy2_len = cpy_ct - cpy1_len;

		memcpy(dst, rx.buf + b_it, cpy1_len);
		memcpy(dst + cpy1_len, rx.buf, cpy2_len);

		rx.p_idx[it] = (b_it + cpy_ct) & (B_SZ(rx) - 1);
		return ct;
	} else {
		return 0;
	}
}

/* advance the packet pointer to the next packet.
 *
 * One must be sure the packet queue is not empty prior to calling this func.
 * That may be done either
 *  - by calling frame_recv_ct (non-zero when the queue is non-empty or
 *  - by only calling this func when one finishes processing the current packet
 *    which ensures that at least one packet (the one presently being
 *    processed) will be in the packet queue.
 */
void frame_recv_next(void)
{
	uint8_t it_1 = CIRC_NEXT(rx.tail, P_SZ(rx));
	rx.tail = it_1;
}

/* return: true if at least one packet is in the queue. The queue includes
 *         the packet currently being processed.
 */
bool frame_recv_have_pkt(void)
{
	return rx.tail != rx.head;
}

/* return: the number of packets presently in the queue. The queue includes
 *         the packet currently being processed.
 */
uint8_t frame_recv_ct(void)
{
	return CIRC_CNT(rx.head, rx.tail, P_SZ(rx));
}

#ifdef DEBUG
# define dprint(fmt, ...) do {		\
	printf(fmt, ## __VA_ARGS__);	\
} while(0)
# define dtxprint(fmt, ...) do {	\
	printf(fmt, ## __VA_ARGS__);	\
	print_packet_buf(&tx);		\
	putchar('\n');			\
} while(0)
# define drxprint(fmt, ...) do {	\
	printf(fmt, ## __VA_ARGS__);	\
	print_packet_buf(&rx);		\
	putchar('\n');			\
} while(0)
# define dprint_wait() print_wait()
#else
# define dtxprint(fmt, ...)
# define dprint(fmt, ...)
# define dprint_wait()
# define drxprint(fmt, ...)
#endif

/** recieve: producer, modifies head **/
RX_ISR()
{
	drxprint("rx_isr: ");
	dprint_wait();

	static bool is_escaped;
	static bool recv_started;
	uint8_t status = RX_STATUS_GET();
	uint8_t data = RX_BYTE_GET();
	static uint16_t crc;

	uint8_t ih = rx.head;
	/* safe location (in rx.p_idx) to store the location of the next
	 * byte to write; */
	uint8_t ih_1 = CIRC_NEXT(rx.head, P_SZ(rx));

	/* check `status` for error conditions */
	if (RX_STATUS_IS_ERROR(status)) {
		/* frame error, data over run, parity error */
		goto drop_packet;
	}

	if (data == START_BYTE) {
		/* prepare for start, reset packet position, etc. */
		/* packet length is non-zero */
		recv_started = true;
		is_escaped = false;

		/* is there any data in the packet? & is its crc valid? */
		if (rx.p_idx[ih] != rx.p_idx[ih_1] && !crc) {
			/* packet has data, check crc. */
			uint8_t ih_2 = CIRC_NEXT(ih_1, P_SZ(rx));
			if (ih_2 == rx.tail) {
				/* no space in p_idx for another packet */

				/* Essentailly a packet drop, but we want
				 * recv_started set as this is a START_BYTE,
				 * after all. */
				rx.p_idx[ih_1] = rx.p_idx[ih];
			} else {
				/* advance the packet idx */
				rx.head = ih_1;

				/* rx.p_idx[next_head] will be set correctly,
				 * update rx.p_idx[next_next_head] to be the
				 * same as rx.p_idx[next_head]
				 */
				rx.p_idx[ih_2] = rx.p_idx[ih_1];
			}
		}

		/* otherwise, we have zero bytes in the packet, no need to
		 * advance */
		crc = CRC_INIT;
		return;
	}

	if (!recv_started) {
		/* ignore stuff until we get a start byte */
		return;
	}

	if (data == RESET_BYTE) {
		goto drop_packet;
	}

	if (data == ESC_BYTE) {
		/* Possible error check: is_escaped should not already
		 * be true */
		is_escaped = true;
		return;
	}

	if (is_escaped) {
		/* Possible error check: is data of one of the allowed
		 * escaped bytes? */
		/* we previously recieved an escape char, transform data */
		is_escaped = false;
		data ^= ESC_MASK;
	}

	crc = _crc_ccitt_update(crc, data);

	/* do we have another byte to write into? */
	uint8_t b_ih_1 = rx.p_idx[ih_1];
	uint8_t b_ih_1_1 = CIRC_NEXT(b_ih_1, B_SZ(rx));
	uint8_t b_it = rx.p_idx[rx.tail];
	if (b_it != b_ih_1_1) {
		rx.buf[b_ih_1] = data;
		rx.p_idx[ih_1] = b_ih_1_1;

		return;
	}

	/* well, shucks. we're out of space, drop the packet */
	/* goto drop_packet; */

drop_packet:
	recv_started = false;
	is_escaped = false;
	crc = CRC_INIT;
	/* first byte of the sequence we are writing to; */
	rx.p_idx[ih_1] = rx.p_idx[ih];
}

/*** Transmision of Data ***/
/** transmit: consumer of data, modifies tail **/
TX_ISR()
{
	/* Only enabled when we have data.
	 * Bytes inseted into location indicated by next_tail.
	 * Tail advanced on packet completion.
	 *
	 * Expectations:
	 *    tx.p_idx[tx.tail] is the next byte to transmit
	 *    tx.p_idx[next_tail] is the end of the currently
	 *        transmitted packet
	 */
	static bool packet_started;
	uint8_t it = tx.tail;
	uint8_t b_it = tx.p_idx[it];
	uint8_t it_1 = CIRC_NEXT(it, P_SZ(tx));

	dtxprint("tx_isr: ");
	dprint_wait();

	if (b_it == tx.p_idx[it_1]) {
		/* no more bytes, signal packet completion */
		/* advance the packet idx. */
		tx.tail = it_1;
		if (it_1 == tx.head) {
			packet_started = false;
			usart0_udre_lock();
		} else {
			packet_started = true;
		}

		TX_BYTE_SEND(START_BYTE);
		return;
	}

	/* Error case for UDRIE enabled when ring empty
	 * no packet indexes seen */
	if (it == tx.head) {
		packet_started = false;
		usart0_udre_isr_off();
		return;
	}

	/* is it a new packet? */
	if (!packet_started) {
		packet_started = true;
		TX_BYTE_SEND(START_BYTE);
		return;
	}

	uint8_t data = tx.buf[b_it];

	if (data == START_BYTE || data == ESC_BYTE || data == RESET_BYTE) {
		TX_BYTE_SEND(ESC_BYTE);
		tx.buf[b_it] = data ^ ESC_MASK;
		return;
	}

	TX_BYTE_SEND(data);

	/* Advance byte pointer */
	tx.p_idx[it] = CIRC_NEXT(b_it, B_SZ(tx));
}

/** transmit: producer of data, modifies head **/
/* 2 API options:
 *  - packet building:
 *     frame_{start,append*,done}
 *  - full packet sending:
 *     frame_send
 */

#define FRAME_DROP(circ, ih_1, ih) do {				\
	(circ).p_idx[ih_1] = (circ).p_idx[ih];			\
	frame_start_flag = false;				\
} while(0)

#define PBUF_APPEND8(circ, val) do {					\
	uint8_t ih = (circ).head;					\
	uint8_t ih_1 = CIRC_NEXT(ih, P_SZ(circ));			\
	uint8_t b_ih_1 = (circ).p_idx[ih_1];				\
	(circ).buf[b_ih_1] = (val);					\
	(circ).p_idx[ih_1] = CIRC_NEXT(b_ih_1, B_SZ(circ));		\
} while(0)

#define PBUF_APPEND16(circ, val) do {					\
	PBUF_APPEND8(circ, (uint8_t)(val >> 8));			\
	PBUF_APPEND8(circ, (uint8_t)(val & 0xff));			\
} while(0)


static bool frame_start_flag;
static uint16_t frame_crc_temp;
void frame_start(void)
{
	if (CIRC_SPACE(tx.head, tx.tail, P_SZ(tx)) < CRC_SZ) {
		return;
	}

	frame_start_flag = true;
	tx.p_idx[CIRC_NEXT(tx.head, P_SZ(tx))] = tx.p_idx[tx.head];
	frame_crc_temp = 0xffff;
}

void frame_append_u8(uint8_t x)
{
	if (!frame_start_flag)
		return;

	uint8_t ih = tx.head;
	uint8_t ih_1 = CIRC_NEXT(ih, P_SZ(tx));
	uint8_t b_ih_1 = tx.p_idx[ih_1];

	/* Can we advance our packet bytes? if not, drop packet */
	if (CIRC_SPACE(b_ih_1, tx.p_idx[tx.tail], B_SZ(tx))
			< (sizeof(uint8_t) + CRC_SZ)) {
		FRAME_DROP(tx, ih_1, ih);
		return;
	}

	frame_crc_temp = _crc_ccitt_update(frame_crc_temp, x);

	PBUF_APPEND8(tx, x);
}

void frame_append_u16(uint16_t x)
{
	if (!frame_start_flag)
		return;

	uint8_t ih = tx.head;
	uint8_t ih_1 = CIRC_NEXT(ih, P_SZ(tx));
	uint8_t b_ih_1 = tx.p_idx[ih_1];

	/* Can we advance our packet bytes? if not, drop packet */
	if (CIRC_SPACE(b_ih_1, tx.p_idx[tx.tail], B_SZ(tx))
			< (sizeof(uint16_t) + CRC_SZ)) {
		FRAME_DROP(tx, ih_1, ih);
		return;
	}

	PBUF_APPEND16(tx, x);

	frame_crc_temp = _crc_ccitt_update(frame_crc_temp, (uint8_t)(x >> 8));
	frame_crc_temp = _crc_ccitt_update(frame_crc_temp, (uint8_t)(x & 0xFF));
}


void frame_done(void)
{
	if (!frame_start_flag)
		return;
	frame_start_flag = false;

	PBUF_APPEND16(tx, frame_crc_temp);

	uint8_t ih = tx.head;
	uint8_t ih_1 = CIRC_NEXT(ih, P_SZ(tx));
	uint8_t ih_2 = CIRC_NEXT(ih_1, P_SZ(tx));

	tx.p_idx[ih_2] = tx.p_idx[ih_1];

	/* XXX: this barrier may not be needed as all necisary updates to
	 * tx.p_idx[ih_1] are done in previous functions */
	barrier();
	tx.head = ih_1;
	usart0_udre_unlock();
}


void frame_send(const void *data, uint8_t nbytes)
{
	uint8_t ih = tx.head;
	uint8_t b_ih = tx.p_idx[ih];
	uint8_t it = tx.tail;
	uint8_t b_it = tx.p_idx[it];

	dtxprint("FRAME_SEND:");

	/* we can fill .buf up completely only in the case that the packet
	 * buffer has more than 1 packet (which is very likely), so use
	 * the standard circ buffer managment here to keep the space open */
	uint8_t space = CIRC_SPACE(b_ih, b_it, B_SZ(tx));

	/* Can we advance our packet bytes? if not, drop packet */
	if ((nbytes + CRC_SZ) > space) {
		dtxprint("\tb space nbytes(%d) + CRC_SZ(%d) > space(%d)",
				nbytes, CRC_SZ, space);
		return;
	}

	uint8_t ih_1 = CIRC_NEXT(ih, P_SZ(tx));
	dprint("\tih_1 = %d\n", ih_1);
	/* do we have space for the packet_idx? */
	if (ih_1 == it) {
		dtxprint("\ti space ih_1(%d) == it(%d)", ih_1, it);
		return;
	}

	uint16_t crc = CRC_INIT;
	{
		/* crc calculation */
		uint8_t i;
		for (i = 0; i < nbytes; i++) {
			crc = _crc_ccitt_update(crc, ((uint8_t *)data)[i]);
		}
	}

	/* amount to copy in first memcpy */
	uint8_t space_to_end =
		MIN(CIRC_SPACE_TO_END(b_ih, b_it, B_SZ(tx)),
				nbytes);

	/* copy first segment of data (may be split) */
	memcpy(tx.buf + b_ih, data, space_to_end);

	/* copy second segment if it exsists (nbytes - space_to_end == 0
	 * when it doesn't) */
	memcpy(tx.buf, data + space_to_end, nbytes - space_to_end);

	/* advance packet length */
	tx.p_idx[ih_1] = (b_ih + nbytes) & (B_SZ(tx) - 1);

	dtxprint("\t BEFORE append:");
	PBUF_APPEND16(tx, crc);
	dtxprint("\t AFTER append:");

	/* update the next I for future packet writes. */
	uint8_t ih_2 = CIRC_NEXT(ih_1, P_SZ(tx));
	tx.p_idx[ih_2] = tx.p_idx[ih_1];

	/* advance packet idx */
	/* XXX: if we usart0_udre_lock() prior to setting tx.head,
	 * the error check in the ISR for an empty packet can be avoided.
	 * As we know that the currently inserted data will not have been
	 * processed, while without the locking if the added packet is short
	 * enough and the ISR is unlocked when we set tx.head, the ISR may be
	 * able to process the entire added packet and disable itself prior
	 * to us calling usart0_udre_unlock().
	 */
	barrier();
	tx.head = ih_1;

	/* new packet starts from tx.p_idx[next_i_head] */
	dtxprint("\tudre_unlock");
	dprint_wait();
	usart0_udre_unlock();
	dprint("\tdone\n");
	dprint_wait();
}


/*** Initialization ***/
#ifdef AVR
static void usart0_init(void)
{
	/* Disable ISRs, recv, and trans */
	UCSR0B = 0;

	/* Asyncronous, parity odd, 1 bit stop, 8 bit data */
	UCSR0C = (0 << UMSEL01) | (0 << UMSEL00)
		| (1 << UPM01)  | (1 << UPM00)
		| (0 << USBS0)
		| (1 << UCSZ01) | (1 << UCSZ00);

	/* Baud 38400 */
# define BAUD 38400
# include <util/setbaud.h>
	UBRR0 = UBRR_VALUE;

# if USE_2X
	UCSR0A = (1 << U2X0);
# else
	UCSR0A = 0;
# endif

	/* Enable RX isr, disable UDRE isr, EN recv and trans, 8 bit data */
	UCSR0B = (1 << RXCIE0) | (0 << UDRIE0)
		| (1 << RXEN0) | (1 << TXEN0)
		| (0 << UCSZ02);

	/* XXX: debugging */
# if defined(DEBUG)
	stdout = stderr = &usart0_io_direct;
# endif
}

void frame_init(void)
{
	usart0_init();
}
#endif /* AVR */
