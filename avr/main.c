#include <string.h>
#include <util/delay.h>
#include <util/crc16.h>

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
		HJA_PKT_ERROR_INITIALIZAER(errnum);	\
	frame_send(&err_pkt, HJA_PL_ERROR);		\
} while(0)

struct encoder_con {
	struct pin enc_a;
	struct pin enc_b;
	uint32_t enc_ct;
} static ec [] = {
	{ PC_4, PC_5, 0},
	{ PC_2, PC_3, 0}
};

#define PC_INIT(pin) do {				\
} while(0)

static void enc_init(void)
{
	uint8_t i;
	for (i = 0; i < ARRAY_SIZE(ec); i++) {
		PC_INIT(ec[i].enc_a);
		PC_INIT(ec[i].enc_b);
	}
}

static int16_t motor_vel[2];

static void update_vel(uint8_t idx, struct hjb_pkt_set_speed *pkt)
{
	motor_vel[idx] = ntohs(pkt->vel[idx]);
	mshb_set(idx, motor_vel[idx]);
	if (motor_vel[idx]) {
		mshb_enable(idx);
	} else {
		mshb_disable(idx);
	}
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
		info.a.current = htons(vals[0]);
		info.b.current = htons(vals[1]);
		info.a.cur_vel = htons(motor_vel[0]);
		info.b.cur_vel = htons(motor_vel[1]);

		frame_send(&info, HJA_PL_INFO);
		break;
	}
	default:
		HJ_SEND_ERROR(1);
		return true;
	}
	return false;
}

#define WDT_PRESCALE ((0 << WDP3) | (1 << WDP2) | (0 << WDP1) | (1 << WDP0))
#define WDT_CSRVAL ((0 << WDE) | (1 << WDIE) | WDT_PRESCALE)

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
		cli();
		WDTCSR = (1 << WDCE) | (1 << WDE) | WDT_CSRVAL;
		WDTCSR = WDT_CSRVAL;
		sei();
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
