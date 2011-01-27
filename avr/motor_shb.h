#ifndef MOTOR_SHB_H_
#define MOTOR_SHB_H_

#include <stdint.h>
#include "muc/muc.h"
#include "muc/timer.h"

struct pin {
	uint8_t volatile *port;
	uint8_t mask;
};

struct pwm16_out {
	uint16_t volatile *mreg; /* the match compare register */
	struct pin p;
};

struct pwm8_out {
	uint8_t volatile *mreg;
	struct pin p;
};

struct mshb {
	/* the atmega328p only has 2 16bit pwms, so we sacrafice pwm
	 * accuracy in the reverse */
	struct pwm16_out pwma;
	struct pwm8_out pwmb;
	struct pin enable;
};

#define PIN_INITIALIZER(port_, idx) { .port = &(port_), .mask = (1 << (idx)) }
#define PWM_INITIALIZER(reg, pin) { .mreg = &(reg), .p = pin }
#define MSHB_INITIALIZER(pa, pb, en)				\
		{ .pwma = pa, .pwmb = pb, .enable = en }

#define PD_4 PIN_INITIALIZER(PORTD, 4) // arduino  4
#define PD_5 PIN_INITIALIZER(PORTD, 5) // arduino  5
#define PD_6 PIN_INITIALIZER(PORTD, 6) // arduino  6
#define PD_7 PIN_INITIALIZER(PORTD, 7) // arduino  7

#define PB_1 PIN_INITIALIZER(PORTB, 1) // arduino  9
#define PB_2 PIN_INITIALIZER(PORTB, 2) // arduino 10

#define PC_2 PIN_INITIALIZER(PORTC, 2) // arduino a2
#define PC_3 PIN_INITIALIZER(PORTC, 3) // arduino a3
#define PC_4 PIN_INITIALIZER(PORTC, 4) // arduino a4
#define PC_5 PIN_INITIALIZER(PORTC, 5) // arduino a5

#define TMR0_PWMA PWM_INITIALIZER(OCR0A, PD_6)
#define TMR0_PWMB PWM_INITIALIZER(OCR0B, PD_5)
#define TMR1_PWMA PWM_INITIALIZER(OCR1A, PB_1)
#define TMR1_PWMB PWM_INITIALIZER(OCR1B, PB_2)

static struct mshb mshb_d [] = {
	MSHB_INITIALIZER(TMR1_PWMA, TMR0_PWMA, PD_4), // A= 9, B=6, INH=4
	MSHB_INITIALIZER(TMR1_PWMB, TMR0_PWMB, PD_7)  // A=10, B=5, INH=7
};

#define PIN_INIT_OUT(pin) do {					\
	PIN_SET_LOW(pin);					\
	*((pin).port - 1) |= (pin).mask;			\
} while(0)

#define PIN_SET_HIGH(pin) do {					\
	*((pin).port) |= (pin).mask;				\
} while(0)

#define PIN_SET_LOW(pin) do {					\
	*((pin).port) &= (uint8_t)~(pin).mask;			\
} while(0)

#define PWM_INIT(pwm) do {					\
	PIN_INIT_OUT((pwm).p);					\
} while(0)

#define MSHB_INIT(mshb) do {					\
	PWM_INIT((mshb).pwma);					\
	PWM_INIT((mshb).pwmb);					\
	PIN_INIT_OUT((mshb).enable);				\
} while(0)

static inline
void pwm8_set(struct pwm8_out pwm, uint16_t val15)
{
	*(pwm.mreg) = (uint8_t)((uint16_t)val15 >> (15-8));
}

static inline
void pwm16_set(struct pwm16_out pwm, uint16_t val15)
{
	*(pwm.mreg) = (uint16_t)val15;
}

static inline
void mshb_init(void)
{
	/* Pin mappings:
	 * Digital 10 / OC1B / PB2 => PA / PWMA / IN (A)
	 * Digital  9 / OC1A / PB1 => PB / PWMB / IN (B)
	 * Digital  7 / PD7 => ENA / ENB / INH (A) / INH (B)
	 */
	TIMER0_INIT_PWM_MAX();
	TIMER1_INIT_PWM(INT16_MAX);
	uint8_t i;
	for (i = 0; i < ARRAY_SIZE(mshb_d); i++) {
		MSHB_INIT(mshb_d[i]);
	}
}

static inline
void mshb_enable(uint8_t i)
{
	PIN_SET_HIGH(mshb_d[i].enable);
}

static inline
void mshb_disable(uint8_t i)
{
	PIN_SET_LOW(mshb_d[i].enable);
}

static inline
void mshb_set(uint8_t i, int16_t speed)
{
	if (speed < 0) {
		pwm16_set(mshb_d[i].pwma, 0);
		pwm8_set(mshb_d[i].pwmb, -speed);
	} else {
		pwm8_set(mshb_d[i].pwmb, 0);
		pwm16_set(mshb_d[i].pwma, speed);
	}
}

#endif
