#include <string.h>
#include <util/delay.h>
#include <util/crc16.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "error_led.h"
#include "frame_async.h"


#include "muc/muc.h"
#include "motor_shb.h"
#include "../hj_proto.h"

static void hj_motor_set_speed(int16_t speed) {
	mshb_set(0, speed);
	mshb_set(1, speed);
}

static inline void hj_parse(uint8_t *buf, uint8_t len)
{
	if (len < HJ_PL_MIN) {
		return;
	}

	{
		/* compute CRC over the length of the packet minus the
		 * crc at the end */
		uint16_t crc = 0xffff;
		uint8_t i;
		for (i = 0; i < (len - 2); i++) {
			crc = _crc_ccitt_update(crc, buf[i]);
		}

		uint16_t recv_crc = (buf[i] << 8) | buf[i+1];

		if (crc != recv_crc)
			return;
	}

	struct hj_pktc_header *head = (typeof(head)) buf;

	switch(head->type) {
	case HJ_PT_SET_SPEED:
		if (len != HJ_PL_SET_SPEED) {
			return;
		}

		struct hj_pkt_set_speed *pkt = (typeof(pkt)) buf;

		hj_motor_set_speed(ntohs(pkt->vel));

		break;
	case HJ_PT_REQ_INFO:
		if (len != HJ_PL_REQ_INFO) {
			return;
		}

		/* send info */
		break;
	}
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
		if (len) {
			hj_parse(buf, len);
		} else {
			ct++;
			if (ct == 0) {
				ct++;
				led_flash(3);
				struct hj_pkt_timeout tout
					= HJ_PKT_TIMEOUT_INITIALIZER;
				frame_send(&tout, HJ_PL_TIMEOUT);
			}
			_delay_ms(70);
		}

	}
}
