/*
 * Arduino
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <avr/io.h>
#include <avr/power.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include <util/atomic.h>
#include <util/parity.h> 
#include <util/delay.h>

#include "usart.h"
#include "clock.h"
#include "msg_proc.h"
#include "version.h"
/*
ISR(BADISR_vect){
}
*/

static inline void init(void)
{
	power_all_disable();

	usart_init();

	sei();
  
	fputs_P(version_str,stdout);
}
__attribute__((noreturn)) void main(void)
{
	init();
	for(;;) {
		if (usart_new_msg()) {
			process_msg();
		}
	}
}

