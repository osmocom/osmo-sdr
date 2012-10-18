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

#include "dma.h"

#define DMA_ENA (1 << 0)
#define DMA_DIS (1 << 0)
#define DMA_SUSP (1 << 8)
#define DMA_KEEPON (1 << 24)

#define AT91C_SRC_DSCR AT91C_HDMA_SRC_DSCR
#define AT91C_DST_DSCR AT91C_HDMA_DST_DSCR
#define AT91C_SRC_INCR AT91C_HDMA_SRC_ADDRESS_MODE
#define AT91C_DST_INCR AT91C_HDMA_DST_ADDRESS_MODE
#define AT91C_SRC_PER AT91C_HDMA_SRC_PER
#define AT91C_DST_PER AT91C_HDMA_DST_PER
#define AT91C_FC AT91C_HDMA_FC

#define AT91C_BTC (0xff <<  0)
#define AT91C_CBTC (0xff <<  8)
#define AT91C_ERR (0xff << 16)

void dma_enable(void)
{
	AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_HDMA;
	AT91C_BASE_HDMA->HDMA_EN = AT91C_HDMA_ENABLE;
}

void dma_enableIrq(uint flag)
{
	AT91C_BASE_HDMA->HDMA_EBCIER = flag;
}

void dma_enableChannel(uint channel)
{
	AT91C_BASE_HDMA->HDMA_CHER |= DMA_ENA << channel;
}

void dma_disableChannel(uint channel)
{
	AT91C_BASE_HDMA->HDMA_CHDR |= DMA_DIS << channel;
}

uint dma_getStatus(void)
{
	return AT91C_BASE_HDMA->HDMA_EBCISR;
}

void dma_setDescriptorAddr(uint channel, uint address)
{
	AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_DSCR = address;
}

void dma_setSourceAddr(uint channel, uint address)
{
	AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_SADDR = address;
}

void dma_setSourceBufferMode(uint channel, uint transferMode, uint addressingType)
{
	uint value;

	value = AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CTRLB;
	value &= ~(AT91C_SRC_DSCR | AT91C_SRC_INCR | 1 << 31);
	switch(transferMode) {
		case DMA_TRANSFER_SINGLE:
			value |= AT91C_SRC_DSCR | addressingType << 24;
			break;
		case DMA_TRANSFER_LLI:
			value |= addressingType << 24;
			break;
		case DMA_TRANSFER_RELOAD:
		case DMA_TRANSFER_CONTIGUOUS:
			value |= AT91C_SRC_DSCR | addressingType << 24 | 1 << 31;
			break;
	}
	AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CTRLB = value;

	if(transferMode == DMA_TRANSFER_RELOAD || transferMode == DMA_TRANSFER_CONTIGUOUS) {
		value = AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CFG;
		AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CFG = value;
	} else {
		AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CFG = 0;
	}
}

void dma_setDestBufferMode(uint channel, uint transferMode, uint addressingType)
{
	uint value;

	value = AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CTRLB;
	value &= ~(uint)(AT91C_DST_DSCR | AT91C_DST_INCR);

	switch(transferMode) {
		case DMA_TRANSFER_SINGLE:
		case DMA_TRANSFER_RELOAD:
		case DMA_TRANSFER_CONTIGUOUS:
			value |= AT91C_DST_DSCR | addressingType << 28;
			break;
		case DMA_TRANSFER_LLI:
			value |= addressingType << 28;
			break;
	}
	AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CTRLB = value;
	if(transferMode == DMA_TRANSFER_RELOAD || transferMode == DMA_TRANSFER_CONTIGUOUS) {
		value = AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CFG;
		AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CFG = value;
	} else {
		AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CFG = 0;
	}
}

void dma_setFlowControl(uint channel, uint flow)
{
	uint value;

	value = AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CTRLB;
	value &= ~(unsigned int)AT91C_FC;
	value |= flow << 21;
	AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CTRLB = value;
}

void dma_setConfiguration(uint channel, uint value)
{
	AT91C_BASE_HDMA->HDMA_CH[channel].HDMA_CFG = value;
}
