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

#include "ssc.h"
#include "../board.h"

void ssc_configure(AT91S_SSC* ssc, uint id, uint bitRate)
{
	// Enable SSC peripheral clock
	AT91C_BASE_PMC->PMC_PCER = 1 << id;

	// Reset, disable receiver & transmitter
	ssc->SSC_CR = AT91C_SSC_RXDIS | AT91C_SSC_TXDIS | AT91C_SSC_SWRST;

	// Configure clock frequency
	if(bitRate != 0)
		ssc->SSC_CMR = BOARD_MCK / (2 * bitRate);
	else ssc->SSC_CMR = 0;
}

void ssc_configureReceiver(AT91S_SSC* ssc, uint rcmr, uint rfmr)
{
	ssc->SSC_RCMR = rcmr;
	ssc->SSC_RFMR = rfmr;
}

void ssc_disableInterrupts(AT91S_SSC* ssc, uint sources)
{
	ssc->SSC_IDR = sources;
}

void ssc_enableInterrupts(AT91S_SSC* ssc, uint sources)
{
	ssc->SSC_IER = sources;
}

void ssc_enableReceiver(AT91S_SSC* ssc)
{
	ssc->SSC_CR = AT91C_SSC_RXEN;
}

void ssc_disableReceiver(AT91S_SSC* ssc)
{
	ssc->SSC_CR = AT91C_SSC_RXDIS;
}
