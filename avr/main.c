#include <string.h>

#include <util/delay.h>
#include <util/crc16.h>
#include <util/atomic.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>

#include "muc/muc.h"
#include "muc/adc.h"
#include "muc/pid.h"

#include "motor_shb.h"
#include "error_led.h"
#include "frame_async.h"
#include "error_frame.h"

#include "../hj_proto.h"

#define MCTRL_PID

const EEMEM struct pid_const_vals pid_ee[2] = {
	PID_K(0xff0lu, 0, 0, 0),
	PID_K(0xff0lu, 0, 0, 0)
};

#ifdef MCTRL_PID
static struct pid mpid[2] = {
	PID_INITIALIZER(0,0,0,0),
	PID_INITIALIZER(0,0,0,0)
};
#endif

/*
 * motor_pwr - stores the present voltage levels being sent to the motors.
 *             When PID is running, this is the value outputed by pid_update.
 */
static int16_t motor_pwr[2];

#define ENC_IN(a, b) { 1 << (a), 1 << (b) }
struct encoder_con {
	uint8_t a;
	uint8_t b;
	uint32_t ct_p;
	uint32_t ct_n;
	int16_t  ct_local;
} static ec_data [] = {
	ENC_IN(PC4, PC5), // PCINT12, PCINT13
	ENC_IN(PC2, PC3)  // PCINT10, PCINT11
};

#define ENC_NAME C
#define ENC_PORT PORT(ENC_NAME)
#define ENC_PIN PIN(ENC_NAME)
#define ENC_ISR PCINT1_vect
#define ENC_PCIE PCIE1
#define ENC_PCMSK PCMSK1

#define e_pc_init(pin) do {					\
	/* set pin as input and unmask in pcint register */	\
	DDR(ENC_NAME) &= ~(pin);				\
	ENC_PCMSK     |=  (pin);				\
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

#define enc_update(e, port, xport) do { \
	uint8_t pa = (e).a;		\
	uint8_t pb = (e).b;		\
	uint8_t a = !(pa & (port));	\
	uint8_t b = !(pb & (port));	\
	if (pa & (xport)) {		\
		if (a != b) {		\
			(e).ct_p ++;	\
			(e).ct_local ++;\
		} else {		\
			(e).ct_n ++;	\
			(e).ct_local --;\
		}			\
	}				\
	if (pb & (xport)) {		\
		if (a == b) {		\
			(e).ct_p ++;	\
			(e).ct_local ++;\
		} else {		\
			(e).ct_n ++;	\
			(e).ct_local --;\
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


static void motor_info_get(struct hj_pktc_motor_info *m, uint16_t current,
		uint8_t i)
{
	m->current = htons(current);
	m->pwr = htons(motor_pwr[i]);
	enc_get(&m->e, i);
}

/* update_pwr - called when the output pwm signal to a motor changes
 *
 * @idx: the index of the motor whose power is changed.
 * @pwr: the new pwm power level for the motor.
 */
static void update_pwr(uint8_t idx, int16_t pwr)
{
	motor_pwr[idx] = pwr;
	mshb_set(idx, pwr);
	/* FIXME: temporarily never-disabled for debugging. */
//	if (pwr) {
		mshb_enable(idx);
//	} else {
//		mshb_disable(idx);
//	}
}

#ifdef MCTRL_PID

#define PID_TIMSK TIMSK2
#define PID_TIMSK_MSK (1 << OCIE2A)
static inline void pid_tmr_off(void)
{
	PID_TIMSK &= ~(PID_TIMSK_MSK);
	barrier();
}

static inline void pid_tmr_on(void)
{
	PID_TIMSK |= PID_TIMSK_MSK;
	barrier();
}

static void pid_k_load(uint8_t i)
{
	eeprom_read_block(&mpid[i].k, &pid_ee[i], sizeof(pid_ee[i]));
}

static void pid_k_load_all(void)
{
	/* load our data */
	pid_k_load(0);
	pid_k_load(1);
}

static void pid_tmr_init(void)
{
	pid_k_load_all();
	TIMER2_INIT_CTC(TIMER2_PSC_1024, 0xff);
}

#define pid_step(m_idx) do {							\
	update_pwr(m_idx, pid_update(&mpid[m_idx], ec_data[m_idx].ct_local));	\
	ec_data[m_idx].ct_local = 0;						\
} while(0)

ISR(TIMER2_COMPA_vect)
{
	pid_step(0);
	pid_step(1);
}

#define pid_k_pack(pkt, m) do {				\
	pkt.k[m].p = htonl(mpid[m].k.p);		\
	pkt.k[m].i = htonl(mpid[m].k.i);		\
	pkt.k[m].d = htonl(mpid[m].k.d);		\
	pkt.k[m].i_max = htons(mpid[m].k.ilimit);	\
} while(0)

#define pid_k_unpack(pkt, m) do {			\
	mpid[m].k.p = ntohl(pkt->k[m].p);		\
	mpid[m].k.i = ntohl(pkt->k[m].i);		\
	mpid[m].k.d = ntohl(pkt->k[m].d);		\
	mpid[m].k.ilimit = ntohs(pkt->k[m].i_max);	\
} while(0)

static void pid_k_store(uint8_t i)
{
	eeprom_update_block(&mpid[i].k, (void *)&pid_ee[i], sizeof(pid_ee[i]));
}

static void pid_k_store_all(void)
{
	pid_k_store(0);
	pid_k_store(1);
}

#endif /* MCTRL_PID */



static void update_vel(struct hjb_pkt_set_speed *pkt)
{
#ifdef MCTRL_PID
	pid_tmr_off();
	pid_set_goal(mpid[0], ntohs(pkt->vel[0]));
	pid_set_goal(mpid[1], ntohs(pkt->vel[1]));
	pid_tmr_on();
#else
	update_pwr(0, ntohs(pkt->vel[0]));
	update_pwr(1, ntohs(pkt->vel[1]));
#endif
}

/** Packet Parsing. **/

#define HJ_CASE(to_from, pkt_name)				\
	case HJ##to_from##_PT_##pkt_name:			\
		if (len != HJ##to_from##_PL_##pkt_name) {	\
			hj_send_error(1);			\
			return true;				\
		}

/* return true = failure */
static bool hj_parse(uint8_t *buf, uint8_t len)
{
	if (len < HJ_PL_MIN) {
		hj_send_error(1);
		return true;
	}

	struct hj_pkt_header *head = (typeof(head)) buf;

	switch(head->type) {
	HJ_CASE(B, SET_SPEED) {
		struct hjb_pkt_set_speed *pkt = (typeof(pkt)) buf;
		update_vel(pkt);
		break;
	}

	HJ_CASE(B, REQ_INFO) {
		uint16_t vals[ADC_CHANNEL_CT];
		adc_val_cpy(vals);

		/* send info */
		struct hja_pkt_info info = HJA_PKT_INFO_INITIALIZER;

		motor_info_get(&info.m[0], vals[0], 0);
		motor_info_get(&info.m[1], vals[1], 1);

		frame_send(&info, HJA_PL_INFO);
		break;
	}
#ifdef MCTRL_PID
	HJ_CASE( , PID_K) {
		struct hj_pkt_pid_k *k = (typeof(k)) buf;

		pid_tmr_off();
		pid_k_unpack(k, 0);
		pid_k_unpack(k, 1);
		pid_tmr_on();
		break;
	}

	HJ_CASE(B, PID_SAVE) {
		pid_k_store_all();
		break;
	}

	HJ_CASE(B, PID_REQ) {
		struct hj_pkt_pid_k k = HJ_PKT_PID_K_INITIALIZER;
		pid_k_pack(k, 0);
		pid_k_pack(k, 1);

		frame_send(&k, HJ_PL_PID_K);
		break;
	}
#endif

	default:
		hj_send_error(head->type);
		return true;
	}
	return false;
}


/* watchdog uses a independent 128kHz oscillator. */

/* Table 10-2.
 * Watchdog Timer Prescale Select
 * WDP[3:0] Number of WDT Oscillator Cycles	Typical Time-out at VCC = 5.0V
 * 0 0 0 0  2K   (2048)				16 ms
 * 0 0 0 1  4K   (4096) cycles			32 ms
 * 0 0 1 0  8K   (8192) cycles			64 ms
 * 0 0 1 1  16K  (16384) cycles			0.125 s
 * 0 1 0 0  32K  (32768) cycles			0.25 s
 * 0 1 0 1  64K  (65536) cycles			0.5 s
 * 0 1 1 0  128K (131072) cycles		1.0 s
 */

/* prescale = 64K cycles ~= 0.5seconds. */
#define WDT_PRESCALE ((0 << WDP3) | (1 << WDP2) | (0 << WDP1) | (1 << WDP0))

/* Table 10-1.
 * Watchdog Timer Configuration
 * WDTON WDE WDIE Action on Time-out
 * 1     0   0    None (stopped)
 * 1     0   1    Interrupt
 * 1     1   0    Reset
 * 1     1   1    Interrupt, then go to System Reset Mode (110 or 0xx)
 * 0     x   x    Reset
 *
 * (1) WDTON is a fuse.
 */
#define WDT_CSRVAL ((0 << WDE) | (1 << WDIE) | WDT_PRESCALE)

static void wdt_setup(void)
{
	wdt_reset();
	MCUSR &= ~(1<<WDRF);
	wdt_reset();

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
#ifdef MCTRL_PID
	pid_tmr_init();
#endif
	sei();

	hj_send_error(10);

	for(;;) {
		uint8_t buf[HJ_PL_MAX];
		uint8_t len = frame_recv_copy(buf, sizeof(buf));
		if (len)
			frame_recv_next();

		if (len && !hj_parse(buf, len)) {
			/* we have recived a valid frame, keep ourselves alive */
			wdt_progress();
		}

		if (wd_timeout) {
			struct hj_pkt_header tout
				= HJA_PKT_TIMEOUT_INITIALIZER;
			frame_send(&tout, HJA_PL_TIMEOUT);
			wd_timeout = false;
		}
	}
}
