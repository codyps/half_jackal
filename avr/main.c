#include <string.h>
#include <util/delay.h>
#include <util/crc16.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "error_led.h"
#include "frame_async.h"


#include "muc/muc.h"
#include "muc/adc.h"
#include "motor_shb.h"
#include "../hj_proto.h"

/* return true = failure */
static bool hj_parse(uint8_t *buf, uint8_t len)
{
	struct hj_pktc_header *head = (typeof(head)) buf;

	switch(head->type) {
	case HJ_PT_SET_SPEED: {
		if (len != HJ_PL_SET_SPEED) {
			led_flash(7);
			return true;
		}

		struct hj_pkt_set_speed *pkt = (typeof(pkt)) buf;

		mshb_set(0, ntohs(pkt->vel_l));
		mshb_set(1, ntohs(pkt->vel_r));

		break;
	}
	case HJ_PT_REQ_INFO: {
		if (len != HJ_PL_REQ_INFO) {
			led_flash(6);
			return true;
		}

		uint16_t vals[ADC_CHANNEL_CT];
		adc_val_cpy(vals);

		/* send info */
		struct hj_pkt_info info = HJ_PKT_INFO_INITIALIZER;
		info.a.current = vals[0];
		info.b.current = vals[1];

		frame_send(&info, HJ_PL_INFO);
		break;
	}
	default:
		led_flash(5);
		return true;
	}
	return false;
}

__attribute__((noreturn))
void main(void)
{
	static uint8_t ct;
	cli();
	frame_init();
	led_init();
	sei();
	for(;;) {
		uint8_t buf[HJ_PL_MAX];
		uint8_t len = frame_recv_copy(buf, sizeof(buf));
		if (len)
			frame_recv_next();
		if (!len || hj_parse(buf, len)) {
			ct++;
			if (ct == 0) {
				ct++;
				led_flash(2);
				struct hj_pkt_timeout tout
					= HJ_PKT_TIMEOUT_INITIALIZER;
				frame_send(&tout, HJ_PL_TIMEOUT);
			}
			_delay_ms(10);
		}
	}
}
