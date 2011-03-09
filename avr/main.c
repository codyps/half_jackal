#include <string.h>

#include <util/delay.h>
#include <util/crc16.h>
#include <util/atomic.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "muc/muc.h"
#include "muc/adc.h"

#include "motor_shb.h"
#include "error_led.h"
#include "frame_async.h"

#include "../hj_proto.h"

#define HJ_SEND_ERROR(errnum) do {			\
	struct hja_pkt_error err_pkt =			\
		HJA_PKT_ERROR_INITIALIZER(errnum);	\
	frame_send(&err_pkt, HJA_PL_ERROR);		\
} while(0)

#define ENC_NAME C
#define ENC_PORT PORT(ENC_NAME)
#define ENC_PIN PIN(ENC_NAME)
#define ENC_ISR PCINT1_vect
#define ENC_PCIE PCIE1
#define ENC_PCMSK PCMSK1

#define ENC_IN(a, b) { 1 << (a), 1 << (b) }
struct encoder_con {
	uint8_t a;
	uint8_t b;
	uint32_t ct_p;
	uint32_t ct_n;
} static ec_data [] = {
	ENC_IN(PC4, PC5), // PCINT12, PCINT13
	ENC_IN(PC2, PC3)  // PCINT10, PCINT11
};


#define e_pc_init(pin) do {					\
	/* set pin as input and unmask in pcint register */	\
	DDR(ENC_NAME)  &= ~(pin);				\
	ENC_PCMSK |=  (pin);				\
} while(0)

#define enc_init_1(e) do {	\
	e_pc_init(e.a);		\
	e_pc_init(e.b);		\
} while(0)

static void enc_init(void)
{
	ENC_PCMSK = 0;

	enc_init_1(ec_data[0]);
	enc_init_1(ec_data[1]);

	PCICR |= (1 << ENC_PCIE);
}

#define enc_isr_off() do {		\
	PCICR &= ~(1 << ENC_PCIE);	\
	barrier();			\
} while(0)

#define enc_isr_on() do {		\
	PCICR |= (1 << ENC_PCIE);	\
	barrier();			\
} while(0)

static void enc_get(struct hj_pktc_enc *e, uint8_t i)
{
	enc_isr_off();
	e->p = htonl(ec_data[i].ct_p);
	e->n = htonl(ec_data[i].ct_n);
	enc_isr_on();
}

static void enc_dec(uint8_t i, struct hj_pktc_enc *e)
{
	uint32_t p = htonl(e->p);
	uint32_t n = htonl(e->n);
	enc_isr_off();
	ec_data[i].ct_p -= p;
	ec_data[i].ct_n -= n;
	enc_isr_on();
}

#define enc_update(e, port, xport) do { \
	uint8_t pa = (e).a;		\
	uint8_t pb = (e).b;		\
	uint8_t a = !(pa & (port));	\
	uint8_t b = !(pb & (port));	\
	if (pa & (xport)) {		\
		if (a != b) {		\
			(e).ct_p ++;	\
		} else {		\
			(e).ct_n ++;	\
		}			\
	}				\
	if (pb & (xport)) {		\
		if (a == b) {		\
			(e).ct_p ++;	\
		} else {		\
			(e).ct_n ++;	\
		}			\
	}				\
} while(0)

ISR(ENC_ISR)
{
	static uint8_t old_port;

	uint8_t port = ENC_PIN;
	uint8_t xport = port ^ old_port;

	enc_update(ec_data[0], port, xport);
	enc_update(ec_data[1], port, xport);
}

static int16_t motor_pwr[2];

static void update_vel(uint8_t idx, struct hjb_pkt_set_speed *pkt)
{
	motor_pwr[idx] = ntohs(pkt->vel[idx]);
	mshb_set(idx, motor_pwr[idx]);
	if (motor_pwr[idx]) {
		mshb_enable(idx);
	} else {
		mshb_disable(idx);
	}
}

static void motor_info_get(struct hj_pktc_motor_info *m, uint16_t current,
		uint8_t i)
{
	m->current = htons(current);
	m->pwr = htons(motor_pwr[i]);
	enc_get(&m->e, i);
}


/* return true = failure */
static bool hj_parse(uint8_t *buf, uint8_t len)
{
	if (len < HJ_PL_MIN) {
		HJ_SEND_ERROR(1);
		return true;
	}

	struct hj_pktc_header *head = (typeof(head)) buf;

	switch(head->type) {
	case HJB_PT_SET_SPEED: {
		if (len != HJB_PL_SET_SPEED) {
			HJ_SEND_ERROR(1);
			return true;
		}

		struct hjb_pkt_set_speed *pkt = (typeof(pkt)) buf;

		update_vel(HJ_MOTOR_R, pkt);
		update_vel(HJ_MOTOR_L, pkt);

		break;
	}
	case HJB_PT_REQ_INFO: {
		if (len != HJB_PL_REQ_INFO) {
			HJ_SEND_ERROR(1);
			return true;
		}

		uint16_t vals[ADC_CHANNEL_CT];
		adc_val_cpy(vals);

		/* send info */
		struct hja_pkt_info info = HJA_PKT_INFO_INITIALIZER;

		motor_info_get(&info.m[0], vals[0], 0);
		motor_info_get(&info.m[1], vals[1], 1);

		frame_send(&info, HJA_PL_INFO);
		break;
	}
	case HJB_PT_ENC_DEC: {
		struct hjb_pkt_enc_dec *ed;
		if (len != sizeof(ed)) {
			HJ_SEND_ERROR(1);
			return true;
		}

		ed = (typeof(ed)) buf;
		enc_dec(0, &ed->e[0]);
		enc_dec(1, &ed->e[1]);
		break;
	}

	default:
		HJ_SEND_ERROR(1);
		return true;
	}
	return false;
}



#define WDT_PRESCALE ((0 << WDP3) | (1 << WDP2) | (0 << WDP1) | (1 << WDP0))
#define WDT_CSRVAL ((1 << WDE) | (1 << WDIE) | WDT_PRESCALE)

static void wdt_setup(void)
{
	wdt_reset();
	MCUSR &= ~(1<<WDRF);

	/* timed sequence: */
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	WDTCSR = WDT_CSRVAL;
}

static volatile bool wd_timeout;

static void wdt_progress(void)
{
	wdt_reset();
	if (!(WDTCSR & (1 << WDIE))) {
		/* enable our interrupt again so we
		 * don't reset on the next expire */
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			WDTCSR = (1 << WDCE) | (1 << WDE) | WDT_CSRVAL;
			WDTCSR = WDT_CSRVAL;
		}
	}
}

ISR(WDT_vect)
{
	wd_timeout = true;
}

__attribute__((noreturn))
void main(void)
{
	cli();
	wdt_setup();
	power_all_disable();
	frame_init();
	adc_init();
	led_init();
	mshb_init();
	enc_init();
	sei();

	HJ_SEND_ERROR(10);

	for(;;) {
		uint8_t buf[HJ_PL_MAX];
		uint8_t len = frame_recv_copy(buf, sizeof(buf));
		if (len)
			frame_recv_next();

		if (len && !hj_parse(buf, len)) {
			wdt_progress();
		}

		if (wd_timeout) {
			struct hja_pkt_timeout tout
				= HJA_PKT_TIMEOUT_INITIALIZER;
			frame_send(&tout, HJA_PL_TIMEOUT);
			wd_timeout = false;
		}
	}
}
