#ifndef INCLUDE_LED_H
#define INCLUDE_LED_H

#include "../at91sam3u4/AT91SAM3U4.h"
#include "../crt/types.h"

void led_configure();

static inline void led_externalSet(Bool on)
{
	if(on)
		AT91C_BASE_PIOB->PIO_CODR = AT91C_PIO_PB18;
	else AT91C_BASE_PIOB->PIO_SODR = AT91C_PIO_PB18;
}

static inline void led_internalSet(Bool on)
{
	if(on)
		AT91C_BASE_PIOB->PIO_CODR = AT91C_PIO_PB19;
	else AT91C_BASE_PIOB->PIO_SODR = AT91C_PIO_PB19;
}

#endif // INCLUDE_LED_H
