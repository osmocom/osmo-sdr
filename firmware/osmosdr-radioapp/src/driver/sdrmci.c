/* (C) 2011-2012 by Christian Daniel <cd@maintech.de>
 * (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include "sdrmci.h"

#if defined(BOARD_SAMPLE_SOURCE_MCI)
#include "mci.h"
#include "irq.h"
#include "dma.h"
#include "pio.h"
#include "dmabuffer.h"
#include "../crt/stdio.h"
#include "../usb/usbdevice.h"
#include "../at91sam3u4/core_cm3.h"

#define DMA_CTRLA (AT91C_HDMA_SRC_WIDTH_WORD | AT91C_HDMA_DST_WIDTH_WORD | AT91C_HDMA_SCSIZE_4 |AT91C_HDMA_DCSIZE_4)
#define DMA_CTRLB (AT91C_HDMA_DST_DSCR_FETCH_FROM_MEM | \
	AT91C_HDMA_DST_ADDRESS_MODE_INCR | \
	AT91C_HDMA_SRC_DSCR_FETCH_DISABLE | \
	AT91C_HDMA_SRC_ADDRESS_MODE_FIXED | \
	AT91C_HDMA_FC_PER2MEM)

#define BTC(n) (1 << n)
#define CBTC(n) (1 << (8 + n))
#define ERR(n) (1 << (16 + n))

__attribute__((section(".buffer"))) static u8 g_bufferSpace[8][2048];

typedef struct {
	Bool running;
	DMABuffer dmaBuffer[8];
	DMABufferQueue emptyQueue;
	DMABufferQueue runningQueue;
	uint overruns;
	uint pending;
	uint completed;
} SDRMCIState;

static SDRMCIState g_mciState;

static const PinConfig mciPins[] = {
	{ AT91C_PIO_PA3, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // MCCK
	{ AT91C_PIO_PA4, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // MCCDA
	{ AT91C_PIO_PA5, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // MCDA0
	{ AT91C_PIO_PA6, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // MCDA1
	{ AT91C_PIO_PA7, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // MCDA2
	{ AT91C_PIO_PA8, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT } // MCDA3
};

static void refillRunningQueue(void)
{
	u32 faultmask = __get_FAULTMASK();
	__disable_fault_irq();

	DMABuffer* buffer;
	DMABuffer* prev = NULL;

	while((buffer = dmaBuffer_dequeue(&g_mciState.emptyQueue)) != NULL) {
		g_mciState.pending++;
		prev = dmaBuffer_enqueue(&g_mciState.runningQueue, buffer);
		buffer->dma.sourceAddress = (uint)&AT91C_BASE_MCI0->MCI_FIFO;
		buffer->dma.destAddress = (uint)buffer->data;
		buffer->dma.controlA = DMA_CTRLA | (buffer->size / 4);
		buffer->dma.controlB = DMA_CTRLB;
		if(prev != NULL)
			prev->dma.nextDescriptor = (uint)&buffer->dma;
		buffer->dma.nextDescriptor = 0;
	}

	__set_FAULTMASK(faultmask);
}

static void submitBuffer(DMABuffer* buffer)
{
	usbDevice_submitBuffer(buffer);
}

void mci0_irqHandler(void)
{
	if(AT91C_BASE_MCI0->MCI_SR & AT91C_MCI_BLKOVRE) {
		g_mciState.overruns++;
	}
}

void hdma_irqHandler(void)
{
	DMABuffer* buffer;
	uint status = dma_getStatus();

	if(status & BTC(BOARD_MCI_DMA_CHANNEL)) {
		while(g_mciState.runningQueue.first->dma.controlA & AT91C_HDMA_DONE) {
			buffer = dmaBuffer_dequeue(&g_mciState.runningQueue);
			g_mciState.pending--;
			g_mciState.completed++;
			submitBuffer(buffer);
		}
		refillRunningQueue();
	}

	if(status & CBTC(BOARD_MCI_DMA_CHANNEL)) {
		mci_stopStream(AT91C_BASE_MCI0);
		g_mciState.running = False;
	}
}

void sdrmci_configure(void)
{
	g_mciState.running = False;
	dmaBuffer_init(&g_mciState.emptyQueue);
	dmaBuffer_init(&g_mciState.runningQueue);
	for(int i = 0; i < 8; i++) {
		for(int j = 0; j < sizeof(g_bufferSpace[i]); j++)
			g_bufferSpace[i][j] = 0x00;
		dmaBuffer_initBuffer(&g_mciState.dmaBuffer[i], g_bufferSpace[i], sizeof(g_bufferSpace[i]));
		dmaBuffer_enqueue(&g_mciState.emptyQueue, &g_mciState.dmaBuffer[i]);
	}
	g_mciState.overruns = 0;
	g_mciState.pending = 0;
	g_mciState.completed = 0;

	// configure PINs for MCI
	pio_configure(mciPins, 6);

	mci_configure(AT91C_BASE_MCI0, AT91C_ID_MCI0);
	mci_enableInterrupts(AT91C_BASE_MCI0, /*(1 << 21) | (1 << 22) |*/ (1 << 24) /*| (1 < 30)*/); // RCOE DTOE BLKOVRE OVRE
	irq_configure(AT91C_ID_MCI0, 0);
	irq_enable(AT91C_ID_MCI0);

	dma_enable();
	irq_configure(AT91C_ID_HDMA, 0);
	irq_enable(AT91C_ID_HDMA);
	dma_enableIrq(BTC(BOARD_MCI_DMA_CHANNEL) | CBTC(BOARD_MCI_DMA_CHANNEL) | ERR(BOARD_MCI_DMA_CHANNEL));

	mci_startStream(AT91C_BASE_MCI0);
}

int sdrmci_dmaStart(void)
{
	u32 faultmask = __get_FAULTMASK();
	__disable_fault_irq();

	refillRunningQueue();

	if(g_mciState.running) {
		__set_FAULTMASK(faultmask);
		return -1;
	}
	if(dmaBuffer_isEmpty(&g_mciState.runningQueue)) {
		__set_FAULTMASK(faultmask);
		return -1;
	}

	dma_disableChannel(BOARD_MCI_DMA_CHANNEL);
	dma_getStatus();
	dma_setDescriptorAddr(BOARD_MCI_DMA_CHANNEL, (uint)&g_mciState.runningQueue.first->dma);
	dma_setSourceAddr(BOARD_MCI_DMA_CHANNEL, (uint)&(AT91C_BASE_MCI0->MCI_FIFO));
	dma_setSourceBufferMode(BOARD_MCI_DMA_CHANNEL, DMA_TRANSFER_LLI, (AT91C_HDMA_SRC_ADDRESS_MODE_FIXED >> 24));
	dma_setDestBufferMode(BOARD_MCI_DMA_CHANNEL, DMA_TRANSFER_LLI, (AT91C_HDMA_DST_ADDRESS_MODE_INCR >> 28));
	dma_setFlowControl(BOARD_MCI_DMA_CHANNEL, AT91C_HDMA_FC_PER2MEM >> 21);
	dma_setConfiguration(BOARD_MCI_DMA_CHANNEL,
		BOARD_MCI_DMA_HW_SRC_REQ_ID |
		BOARD_MCI_DMA_HW_DEST_REQ_ID |
		AT91C_HDMA_SRC_H2SEL_HW |
		AT91C_HDMA_DST_H2SEL_SW |
		AT91C_HDMA_SOD_DISABLE |
		AT91C_HDMA_FIFOCFG_LARGESTBURST);

	g_mciState.running = True;
	dma_enableChannel(BOARD_MCI_DMA_CHANNEL);

	__set_FAULTMASK(faultmask);
	return 0;
}

void sdrmci_returnBuffer(DMABuffer* buffer)
{
	dmaBuffer_enqueue(&g_mciState.emptyQueue, buffer);
	sdrmci_dmaStart();
}

void sdrmci_printStats(void)
{
	printf("FPGA MCI statistics: running: %s, completed: %u, pending: %u, overruns: %u\n",
		g_mciState.running ? "yes" : "no",
		g_mciState.completed,
		g_mciState.pending,
		g_mciState.overruns);
}

#else
void mci0_irqHandler(void)
{
	dprintf("MCI0 irq should not happen\n");
}
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)
