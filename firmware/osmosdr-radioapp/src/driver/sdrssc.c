/* (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
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
#include "sdrssc.h"
#include "../crt/stdio.h"

#if defined(BOARD_SAMPLE_SOURCE_SSC)
#include "ssc.h"
#include "irq.h"
#include "dma.h"
#include "pio.h"
#include "dmabuffer.h"
#include "../dsp/arm_math.h"
#include "dbgio.h"
#include "../usb/usbdevice.h"

#define DMA_CTRLA (AT91C_HDMA_SRC_WIDTH_WORD | AT91C_HDMA_DST_WIDTH_WORD | AT91C_HDMA_SCSIZE_1 |AT91C_HDMA_DCSIZE_4)
#define DMA_CTRLB (AT91C_HDMA_DST_DSCR_FETCH_FROM_MEM | \
	AT91C_HDMA_DST_ADDRESS_MODE_INCR | \
	AT91C_HDMA_SRC_DSCR_FETCH_DISABLE | \
	AT91C_HDMA_SRC_ADDRESS_MODE_FIXED | \
	AT91C_HDMA_FC_PER2MEM)

#define BTC(n) (1 << n)
#define CBTC(n) (1 << (8 + n))
#define ERR(n) (1 << (16 + n))

__attribute__((section(".buffer"))) static u8 g_bufferSpace[4][4096];

typedef struct {
	Bool running;
	DMABuffer dmaBuffer[4];
	DMABufferQueue emptyQueue;
	DMABufferQueue runningQueue;
	q31_t fftBuffer[128];
	volatile Bool fillFFTBuffer;
	uint overruns;
	uint pending;
	uint completed;
} SDRSSCState;

static SDRSSCState g_sscState;

static const PinConfig sscPins[] = {
	{ AT91C_PIO_PA26, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // TD
	{ AT91C_PIO_PA27, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // RD
	{ AT91C_PIO_PA28, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // TK
	{ AT91C_PIO_PA29, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // RK
	{ AT91C_PIO_PA30, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // TF
	{ AT91C_PIO_PA31, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT } // RF
};

static void refillRunningQueue(void)
{
	u32 faultmask = __get_FAULTMASK();
	__disable_fault_irq();

	DMABuffer* buffer;
	DMABuffer* prev = NULL;

	while((buffer = dmaBuffer_dequeue(&g_sscState.emptyQueue)) != NULL) {
		g_sscState.pending++;
		prev = dmaBuffer_enqueue(&g_sscState.runningQueue, buffer);
		buffer->dma.sourceAddress = (uint)&AT91C_BASE_SSC0->SSC_RHR;
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
	if(g_sscState.fillFFTBuffer) {
		g_sscState.fillFFTBuffer = False;
		for(int i = 0; i < 128; i++)
			g_sscState.fftBuffer[i] = ((s16*)buffer->data)[i] << 15;
	}
	usbDevice_submitBuffer(buffer);
}

void ssc0_irqHandler(void)
{
	if(AT91C_BASE_SSC0->SSC_SR & AT91C_SSC_OVRUN)
		g_sscState.overruns++;
}

void hdma_irqHandler(void)
{
	DMABuffer* buffer;
	uint status = dma_getStatus();

	if(status & BTC(BOARD_SSC_DMA_CHANNEL)) {
		while(g_sscState.runningQueue.first->dma.controlA & AT91C_HDMA_DONE) {
			buffer = dmaBuffer_dequeue(&g_sscState.runningQueue);
			g_sscState.pending--;
			g_sscState.completed++;
			submitBuffer(buffer);
		}
		refillRunningQueue();
	}

	if(status & CBTC(BOARD_SSC_DMA_CHANNEL)) {
		ssc_disableReceiver(AT91C_BASE_SSC0);
		g_sscState.running = False;
	}
}

int sdrssc_dmaStart(void)
{
	u32 faultmask = __get_FAULTMASK();
	__disable_fault_irq();

	refillRunningQueue();

	if(g_sscState.running) {
		__set_FAULTMASK(faultmask);
		return -1;
	}
	if(dmaBuffer_isEmpty(&g_sscState.runningQueue)) {
		__set_FAULTMASK(faultmask);
		return -1;
	}

	dma_disableChannel(BOARD_SSC_DMA_CHANNEL);
	dma_getStatus();
	dma_setDescriptorAddr(BOARD_SSC_DMA_CHANNEL, (uint)&g_sscState.runningQueue.first->dma);
	dma_setSourceAddr(BOARD_SSC_DMA_CHANNEL, (uint)&AT91C_BASE_SSC0->SSC_RHR);
	dma_setSourceBufferMode(BOARD_SSC_DMA_CHANNEL, DMA_TRANSFER_LLI, (AT91C_HDMA_SRC_ADDRESS_MODE_FIXED >> 24));
	dma_setDestBufferMode(BOARD_SSC_DMA_CHANNEL, DMA_TRANSFER_LLI, (AT91C_HDMA_DST_ADDRESS_MODE_INCR >> 28));
	dma_setFlowControl(BOARD_SSC_DMA_CHANNEL, AT91C_HDMA_FC_PER2MEM >> 21);
	dma_setConfiguration(BOARD_SSC_DMA_CHANNEL,
		BOARD_SSC_DMA_HW_SRC_REQ_ID |
		BOARD_SSC_DMA_HW_DEST_REQ_ID |
		AT91C_HDMA_SRC_H2SEL_HW |
		AT91C_HDMA_DST_H2SEL_SW |
		AT91C_HDMA_SOD_DISABLE |
		AT91C_HDMA_FIFOCFG_LARGESTBURST);

	g_sscState.running = True;
	dma_enableChannel(BOARD_SSC_DMA_CHANNEL);
	ssc_enableReceiver(AT91C_BASE_SSC0);

	__set_FAULTMASK(faultmask);
	return 0;
}

void sdrssc_dmaStop(void)
{
	u32 faultmask = __get_FAULTMASK();
	__disable_fault_irq();

	ssc_disableReceiver(AT91C_BASE_SSC0);
	g_sscState.running = False;

	/* clear any pending interrupts */
	dma_disableChannel(BOARD_SSC_DMA_CHANNEL);
	dma_getStatus();

	__set_FAULTMASK(faultmask);
}

void sdrssc_returnBuffer(DMABuffer* buffer)
{
	dmaBuffer_enqueue(&g_sscState.emptyQueue, buffer);
	sdrssc_dmaStart();
}

void sdrssc_configure(void)
{
	pio_configure(sscPins, 6);

	g_sscState.running = False;
	dmaBuffer_init(&g_sscState.emptyQueue);
	dmaBuffer_init(&g_sscState.runningQueue);
	for(int i = 0; i < 4; i++) {
		dmaBuffer_initBuffer(&g_sscState.dmaBuffer[i], g_bufferSpace[i], sizeof(g_bufferSpace[i]));
		dmaBuffer_enqueue(&g_sscState.emptyQueue, &g_sscState.dmaBuffer[i]);
	}
	g_sscState.fillFFTBuffer = False;
	g_sscState.overruns = 0;
	g_sscState.pending = 0;
	g_sscState.completed = 0;

	ssc_disableReceiver(AT91C_BASE_SSC0);
	ssc_configure(AT91C_BASE_SSC0, AT91C_ID_SSC0, 0);
	ssc_configureReceiver(AT91C_BASE_SSC0,
		AT91C_SSC_CKS_RK |
		AT91C_SSC_CKO_NONE |
		AT91C_SSC_CKG_NONE |
		AT91C_SSC_START_RISE_RF |
		AT91C_SSC_CKI,
		AT91C_SSC_MSBF | (32 - 1));
	ssc_disableInterrupts(AT91C_BASE_SSC0, 0xffffffff);

	ssc_enableInterrupts(AT91C_BASE_SSC0, AT91C_SSC_OVRUN);
	irq_configure(AT91C_ID_SSC0, 0);
	irq_enable(AT91C_ID_SSC0);

	dma_enable();
	irq_configure(AT91C_ID_HDMA, 0);
	irq_enable(AT91C_ID_HDMA);
	dma_enableIrq(BTC(BOARD_SSC_DMA_CHANNEL) | CBTC(BOARD_SSC_DMA_CHANNEL) | ERR(BOARD_SSC_DMA_CHANNEL));
}

void sdrssc_printStats(void)
{
	printf("FPGA SSC statistics: running: %s, completed: %u, pending: %u, overruns: %u\n",
		g_sscState.running ? "yes" : "no",
		g_sscState.completed,
		g_sscState.pending,
		g_sscState.overruns);
}

void sdrssc_fft(void)
{
	q31_t magnitude[64];

	if(!g_sscState.running) {
		printf("SSC FFT: not running\n");
		return;
	}

	g_sscState.fillFFTBuffer = True;
	while(g_sscState.fillFFTBuffer) ;

	arm_cfft_radix4_instance_q31 fft;
	arm_status status;
	status = arm_cfft_radix4_init_q31(&fft, 64, 0, 1);
	arm_cfft_radix4_q31(&fft, g_sscState.fftBuffer);
	arm_cmplx_mag_q31(g_sscState.fftBuffer, magnitude, 64);

	for(int i = 0; i < 64; i++) {
		for(int j = 0; j < 31; j++) {
			u32 ref = 1 << j;
			if(magnitude[i] > ref) {
				continue;
			} else {
				magnitude[i] = j;
				break;
			}
		}
	}

	for(int i = 16; i >= 0; i--) {
		for(int j = 0; j < 64; j++) {
			int idx = (j + 32) % 64;
			if(magnitude[idx] >= i * 2) {
				if(magnitude[idx] >= (i * 2 + 1))
					dprintf("|");
				else dprintf(".");
			} else dprintf(" ");
		}
		dprintf("\n");
	}
	printf("\n");

#if 0
	while(!dbgio_isRxReady()) {
		g_sscState.fillFFTBuffer = True;
		while(g_sscState.fillFFTBuffer) ;

		arm_cfft_radix4_instance_q31 fft;
		arm_status status;
		status = arm_cfft_radix4_init_q31(&fft, 64, 0, 1);
		arm_cfft_radix4_q31(&fft, g_sscState.fftBuffer);
		arm_cmplx_mag_q31(g_sscState.fftBuffer, magnitude, 64);

		for(int i = 0; i < 64; i++) {
			for(int j = 0; j < 31; j++) {
				u32 ref = 1 << j;
				if(magnitude[i] > ref) {
					continue;
				} else {
					magnitude[i] = j;
					break;
				}
			}
		}

		for(int i = 0; i < 64; i++) {
			//                        0123456789
			//const static char* b = " ,-:*+=%@#";
			const static char* b = " .-:+=%*#@";
			int idx = (i + 32) % 64;
			int v = magnitude[idx] / 2;
			if(v < 0)
				v = 0;
			else if(v > 9)
				v = 9;
			dbgio_putCharDirect(b[magnitude[idx] / 3]);
		}
		dprintf("\n");
	}
	while(dbgio_getChar() >= 0);
	printf("\n");
#endif
}

#else
void ssc0_irqHandler(void)
{
	dprintf("SSC0 irq should not happen\n");
}
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
