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

#include "irq.h"
#include "../at91sam3u4/AT91SAM3U4.h"
#include "../board.h"
#include "../at91sam3u4/core_cm3.h"

void irq_configure(uint source, uint priority)
{
	uint priGroup = __NVIC_PRIO_BITS;
	uint nPre = 8 - priGroup;
	uint nSub = priGroup;
	uint preemptionPriority;
	uint subPriority;
	uint irqPriority;

	preemptionPriority = (priority & 0xff00) >> 8;
	subPriority = (priority & 0xff);

	// Disable the interrupt first
	NVIC_DisableIRQ((IRQn_Type)source);

	// Clear any pending status
	NVIC_ClearPendingIRQ((IRQn_Type)source);

	if(subPriority >= (0x01 << nSub))
		subPriority = (0x01 << nSub) - 1;
	if(preemptionPriority >= (0x01 << nPre))
		preemptionPriority = (0x01 << nPre) - 1;

	irqPriority = (subPriority | (preemptionPriority << nSub));
	NVIC_SetPriority((IRQn_Type)source, irqPriority);
}

void irq_enable(uint source)
{
	NVIC_EnableIRQ((IRQn_Type)source);
}

void irq_disable(uint source)
{
	NVIC_DisableIRQ((IRQn_Type)source);
}
