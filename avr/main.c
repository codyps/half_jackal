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

static uint16_t crc_ccitt_buf(uint16_t crc, void *buf_v, uint8_t len)
{
	uint8_t i;
	uint8_t *buf = buf_v;
	for (i = 0; i < len ; i++) {
		crc = _crc_ccitt_update(crc, buf[i]);
	}
	return crc;
}

static void hj_parse(uint8_t *buf, uint8_t len)
{
	if (len < HJ_PL_MIN) {
		return;
	}

	{
		/* compute CRC over the length of the packet minus the
		 * crc at the end */
		uint16_t crc = crc_ccitt_buf(0xffff, buf, len - 2);
		uint16_t recv_crc = (buf[len-2] << 8) | buf[len-1];

		if (crc != recv_crc)
			return;
	}

	struct hj_pktc_header *head = (typeof(head)) buf;

	switch(head->type) {
	case HJ_PT_SET_SPEED: {
		if (len != HJ_PL_SET_SPEED) {
			return;
		}

		struct hj_pkt_set_speed *pkt = (typeof(pkt)) buf;

		mshb_set(0, ntohs(pkt->vel_l));
		mshb_set(1, ntohs(pkt->vel_r));

		break;
	}
	case HJ_PT_REQ_INFO: {
		if (len != HJ_PL_REQ_INFO) {
			return;
		}

		uint16_t vals[ADC_CHANNEL_CT];
		adc_val_cpy(vals);

		/* send info */
		struct hj_pkt_info info = HJ_PKT_INFO_INITIALIZER;
		info.a.current = vals[0];
		info.b.current = vals[1];

		info.crc = crc_ccitt_buf(0xffff, &info, HJ_PL_INFO - 2);

		frame_send(&info, HJ_PL_INFO);
		break;
	}
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
