#ifndef ADC_CONF_H_
#define ADC_CONF_H_

#include <stdint.h>
#include <muc/clock.h>

/* ADC Prescale calculation (2^n | uint n < 8 ) */
/* From datasheet. */
#define ADC_MAX_CLK KHz(200L)

static const uint8_t adc_chan_map[] = { 0, 1 };

#endif /*_ADC_CONF_H_*/
