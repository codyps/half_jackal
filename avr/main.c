#include <string.h>
#include <util/delay.h>
#include <util/crc16.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "error_led.h"
#include "frame_async.h"


#include "muc/muc.h"
#include "muc/adc.h"
#include "motor_shb.h"
#include "../hj_proto.h"

#define HJ_SEND_ERROR(errnum) do {			\
	struct hja_pkt_error err_pkt =			\
		HJA_PKT_ERROR_INITIALIZAER(errnum);	\
	frame_send(&err_pkt, HJA_PL_ERROR);		\
} while(0)

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

		mshb_enable(0);
		mshb_enable(1);
		mshb_set(0, ntohs(pkt->vel_l));
		mshb_set(1, ntohs(pkt->vel_r));

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
		info.a.current = vals[0];
		info.b.current = vals[1];

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

volatile bool wd_timeout;

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
	led_init();
	mshb_init();
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
