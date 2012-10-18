/* (C) 2011-2012 by Christian Daniel <cd@maintech.de>
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

#include "dbgio.h"
#include "../at91sam3u4/AT91SAM3U4.h"
#include "../board.h"
#include "../at91sam3u4/core_cm3.h"
#include "irq.h"

#define FIFO_SIZE_BITS 7

typedef struct DebugUart_ {
	u8 rxFifo[1 << FIFO_SIZE_BITS];
	u8 txFifo[1 << FIFO_SIZE_BITS];
	uint rxWPos;
	uint rxRPos;
	uint txWPos;
	volatile uint txRPos;
} DebugUart;

static DebugUart g_debugUart;

void dbgio_irqHandler(void)
{
	u8 c;
	uint wPos;
	uint csr = AT91C_BASE_DBGU->DBGU_CSR & AT91C_BASE_DBGU->DBGU_IMR;

	// receive data register full
	if(csr & AT91C_US_RXRDY) {
		// read character
		c = AT91C_BASE_DBGU->DBGU_RHR;

		// put character into FIFO
		wPos = (g_debugUart.rxWPos + 1) % (1 << FIFO_SIZE_BITS);
		if(wPos != g_debugUart.rxRPos) {
			g_debugUart.rxFifo[g_debugUart.rxWPos] = c;
			g_debugUart.rxWPos = wPos;
		}
	}

	// transmit data register empty
	if(csr & AT91C_US_TXEMPTY) {
		// check if we have more to send
		if(g_debugUart.txWPos != g_debugUart.txRPos) {
			c = g_debugUart.txFifo[g_debugUart.txRPos];
			g_debugUart.txRPos = (g_debugUart.txRPos + 1) % (1 << FIFO_SIZE_BITS);

			// write character
			AT91C_BASE_DBGU->DBGU_THR = c;
		} else {
			// no more bytes to send -> switch off interrupt
			AT91C_BASE_DBGU->DBGU_IDR = AT91C_US_TXEMPTY;
		}
	}
}

void dbgio_configure(uint baudrate)
{
	// enable clock for UART
	AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_DBGU);

	// configure PINs for DBGU
	// RXD
	AT91C_BASE_PIOA->PIO_IDR = AT91C_PIO_PA11;
	AT91C_BASE_PIOA->PIO_PPUDR = AT91C_PIO_PA11;
	AT91C_BASE_PIOA->PIO_MDDR = AT91C_PIO_PA11;
	AT91C_BASE_PIOA->PIO_ODR = AT91C_PIO_PA11;
	AT91C_BASE_PIOA->PIO_PDR = AT91C_PIO_PA11;
	AT91C_BASE_PIOA->PIO_ABSR &= ~AT91C_PIO_PA11;
	// TXD
	AT91C_BASE_PIOA->PIO_IDR = AT91C_PIO_PA12;
	AT91C_BASE_PIOA->PIO_PPUDR = AT91C_PIO_PA12;
	AT91C_BASE_PIOA->PIO_MDDR = AT91C_PIO_PA12;
	AT91C_BASE_PIOA->PIO_ODR = AT91C_PIO_PA12;
	AT91C_BASE_PIOA->PIO_PDR = AT91C_PIO_PA12;
	AT91C_BASE_PIOA->PIO_ABSR &= ~AT91C_PIO_PA12;

	// reset & disable receiver and transmitter, disable interrupts
	AT91C_BASE_DBGU->DBGU_CR = AT91C_US_RSTRX | AT91C_US_RSTTX;
	AT91C_BASE_DBGU->DBGU_IDR = 0xffffffff;

	// Configure baud rate
	AT91C_BASE_DBGU->DBGU_BRGR = BOARD_MCK / (baudrate * 16);

	// Configure mode register
	AT91C_BASE_DBGU->DBGU_MR = AT91C_US_PAR_NONE;

	// Disable DMA channel
	AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_RXTDIS | AT91C_PDC_TXTDIS;

	// setup FIFO
	g_debugUart.rxWPos = 0;
	g_debugUart.rxRPos = 0;
	g_debugUart.txWPos = 0;
	g_debugUart.txRPos = 0;

	// configure IRQ
	irq_configure(AT91C_ID_DBGU, 0);
	irq_enable(AT91C_ID_DBGU);
	AT91C_BASE_DBGU->DBGU_IER = AT91C_DBGU_RXRDY;

	// Enable receiver and transmitter
	AT91C_BASE_DBGU->DBGU_CR = AT91C_US_RXEN | AT91C_US_TXEN;
}

int dbgio_getChar(void)
{
	u32 faultmask = __get_FAULTMASK();
	int res;

	__disable_fault_irq();

	// if we have a character, get it and advance FIFO
	if(g_debugUart.rxWPos != g_debugUart.rxRPos) {
		res = g_debugUart.rxFifo[g_debugUart.rxRPos];
		g_debugUart.rxRPos = (g_debugUart.rxRPos + 1) % (1 << FIFO_SIZE_BITS);
	} else {
		// otherwise, return -1
		res = -1;
	}

	__set_FAULTMASK(faultmask);

	return res;
}

void dbgio_putChar(u8 c)
{
	u32 faultmask = __get_FAULTMASK();
	uint wPos;

	__disable_fault_irq();

	// if uart is running, feed the character to the FIFO
	if(AT91C_BASE_DBGU->DBGU_IMR & AT91C_DBGU_TXEMPTY) {
		// check if there is space in the FIFO
		wPos = (g_debugUart.txWPos + 1) % (1 << FIFO_SIZE_BITS);
		if(wPos == g_debugUart.txRPos) {
			// FIFO full - wait for a free space
			if(!faultmask) {
				// ...but only if we're not in an IRQ
				__set_FAULTMASK(faultmask);
				while(wPos == g_debugUart.txRPos) ;
				__disable_fault_irq();
			} else {
				// sorry, can't help here
				return;
			}
		}
		// check if it is still running - otherwise fall through and restart UART
		if(AT91C_BASE_DBGU->DBGU_IMR & AT91C_DBGU_TXEMPTY) {
			// feed characater into FIFO
			g_debugUart.txFifo[g_debugUart.txWPos] = c;
			g_debugUart.txWPos = wPos;
			__set_FAULTMASK(faultmask);
			return;
		}
	}
	// send directly and start FIFO
	AT91C_BASE_DBGU->DBGU_IER = AT91C_US_TXEMPTY;
	AT91C_BASE_DBGU->DBGU_THR = c;

	__set_FAULTMASK(faultmask);
}

void dbgio_putCharDirect(u8 c)
{
	while ((AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXEMPTY) == 0);
	AT91C_BASE_DBGU->DBGU_THR = c;
}

Bool dbgio_isRxReady(void)
{
	u32 faultmask = __get_FAULTMASK();
	Bool res;

	__disable_fault_irq();

	res = (g_debugUart.rxWPos != g_debugUart.rxRPos) ? True : False;

	__set_FAULTMASK(faultmask);

	return res;
}

void dbgio_flushOutput(void)
{
	while(g_debugUart.txWPos != g_debugUart.txRPos) __NOP();
}
