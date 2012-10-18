/* This file uses lots of code from the Atmel reference library:
 * ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2008, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

#include "pio.h"

static void setPeripheralA(AT91S_PIO* pio, u32 mask, Bool enablePullUp)
{
#if !defined(AT91C_PIOA_ASR)
	unsigned int abmr;
#endif
	// Disable interrupts on the pin(s)
	pio->PIO_IDR = mask;
	// Enable the pull-up(s) if necessary
	if(enablePullUp)
		pio->PIO_PPUER = mask;
	else pio->PIO_PPUDR = mask;

	// Configure pin
#if defined(AT91C_PIOA_ASR)
	pio->PIO_ASR = mask;
#else
	abmr = pio->PIO_ABSR;
	pio->PIO_ABSR &= (~mask & abmr);
#endif
	pio->PIO_PDR = mask;
}

static void setPeripheralB(AT91S_PIO* pio, u32 mask, Bool enablePullUp)
{
#if !defined(AT91C_PIOA_BSR)
	unsigned int abmr;
#endif

	// Disable interrupts on the pin(s)
	pio->PIO_IDR = mask;

	// Enable the pull-up(s) if necessary
	if(enablePullUp)
		pio->PIO_PPUER = mask;
	else pio->PIO_PPUDR = mask;

	// Configure pin
#if defined(AT91C_PIOA_BSR)
	pio->PIO_BSR = mask;
#else
	abmr = pio->PIO_ABSR;
	pio->PIO_ABSR = mask | abmr;
#endif
	pio->PIO_PDR = mask;
}

static void setInput(AT91S_PIO* pio, u32 mask, Bool enablePullUp, Bool enableFilter)
{
	// Disable interrupts
	pio->PIO_IDR = mask;

	// Enable pull-up(s) if necessary
	if(enablePullUp)
		pio->PIO_PPUER = mask;
	else pio->PIO_PPUDR = mask;

	// Enable filter(s) if necessary
	if(enableFilter)
		pio->PIO_IFER = mask;
	else pio->PIO_IFDR = mask;
	// Configure pin as input
	pio->PIO_ODR = mask;
	pio->PIO_PER = mask;
}

static void setOutput(AT91S_PIO* pio, u32 mask, Bool defaultValue, Bool enableMultiDrive, Bool enablePullUp)
{
	// Disable interrupts
	pio->PIO_IDR = mask;
	// Enable pull-up(s) if necessary
	if(enablePullUp)
		pio->PIO_PPUER = mask;
	else pio->PIO_PPUDR = mask;

	// Enable multi-drive if necessary
	if(enableMultiDrive)
		pio->PIO_MDER = mask;
	else pio->PIO_MDDR = mask;

	// Set default value
	if(defaultValue)
		pio->PIO_SODR = mask;
	else pio->PIO_CODR = mask;

	// Configure pin(s) as output(s)
	pio->PIO_OER = mask;
	pio->PIO_PER = mask;
}

void pio_configure(const PinConfig* list, uint size)
{
	while(size > 0) {
		// enable clock for PIO
		AT91C_BASE_PMC->PMC_PCER = (1 << list->id);

		switch(list->type) {
			case PIO_PERIPH_A:
				setPeripheralA(list->pio, list->mask, (list->attribute & PIO_PULLUP) ? True : False);
				break;

			case PIO_PERIPH_B:
				setPeripheralB(list->pio, list->mask, (list->attribute & PIO_PULLUP) ? True : False);
				break;

			case PIO_INPUT:
				AT91C_BASE_PMC->PMC_PCER = 1 << list->id;
				setInput(list->pio, list->mask, (list->attribute & PIO_PULLUP) ? True : False, (list->attribute & PIO_DEGLITCH) ? True : False);
#if defined(AT91C_PIOA_IFDGSR) //PIO3 with Glitch or Debouncing selection
				//if glitch input filter enabled, set it
#if 0
				if(list->attribute & PIO_DEGLITCH) //Glitch input filter enabled
				setFilter(list->pio, list->inFilter.filterSel, list->inFilter.clkDivider);
#endif
#endif
				break;

			case PIO_OUTPUT_0:
			case PIO_OUTPUT_1:
				setOutput(
					list->pio,
					list->mask,
					(list->type == PIO_OUTPUT_1) ? True : False,
					(list->attribute & PIO_OPENDRAIN) ? True : False,
					(list->attribute & PIO_PULLUP) ? True : False);
				break;

			default:
				break;
		}

		list++;
		size--;
	}
}

void pio_set(const PinConfig* pin)
{
	pin->pio->PIO_SODR = pin->mask;
}

void pio_clear(const PinConfig* pin)
{
	pin->pio->PIO_CODR = pin->mask;
}

uint pio_get(const PinConfig* pin)
{
	uint reg;

	if(((pin->type == PIO_OUTPUT_0) || (pin->type == PIO_OUTPUT_1)) && (!(pin->attribute & PIO_OPENDRAIN)))
		reg = pin->pio->PIO_ODSR;
	else reg = pin->pio->PIO_PDSR;

	if((reg & pin->mask) == 0)
		return 0;
	else return 1;
}
