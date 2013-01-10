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

#include <stddef.h>
#include "../crt/types.h"
#include "../board.h"
#include "../crt/stdio.h"
#include "twi.h"
#include "pio.h"

#define TWI_TIMEOUT_MAX 50000

static const PinConfig twi0PinsTWI[] = {
	{ AT91C_PIO_PA9, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // SDA
	{ AT91C_PIO_PA10, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT } // SCL
};

static const PinConfig twi0PinsPIO[] = {
	{ AT91C_PIO_PA9, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_OUTPUT_1, PIO_OPENDRAIN | PIO_PULLUP }, // SDA
	{ AT91C_PIO_PA10, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_OUTPUT_1, PIO_OPENDRAIN | PIO_PULLUP } // SCL
};

static const PinConfig twi1PinsPIO[] = {
	{ AT91C_PIO_PA24, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_OUTPUT_1, PIO_OPENDRAIN }, // SDA
	{ AT91C_PIO_PA25, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_OUTPUT_1, PIO_OPENDRAIN } // SCL
};

static void bitDelay(void)
{
	for(volatile uint i = 0; i < 3000; i++) ;
}

#define SDA0 &twi0PinsPIO[0]
#define SCL0 &twi0PinsPIO[1]

#define SDA1 &twi1PinsPIO[0]
#define SCL1 &twi1PinsPIO[1]

static void pioWriteBit(uint b, const PinConfig* sda, const PinConfig* scl)
{
	if(b)
		pio_set(sda);
	else pio_clear(sda);
	pio_set(scl);
	bitDelay();
	while(pio_get(scl) == 0) ;
	pio_clear(scl);
	bitDelay();
	if(b)
		pio_clear(sda);
}

static uint pioReadBit(const PinConfig* sda, const PinConfig* scl)
{
	pio_set(sda);
	pio_set(scl);
	bitDelay();
	while(pio_get(scl) == 0) ;
	uint res = pio_get(sda);
	pio_clear(scl);
	bitDelay();

	return res;
}

static void pioStart(const PinConfig* sda, const PinConfig* scl)
{
	pio_set(sda);
	pio_set(scl);
	bitDelay();
	while(pio_get(scl) == 0) ;
	pio_clear(sda);
	bitDelay();
	pio_clear(scl);
	bitDelay();
}

// Send a STOP Condition
//
static void pioStop(const PinConfig* sda, const PinConfig* scl)
{
	pio_clear(sda);
	pio_set(scl);
	bitDelay();
	while(pio_get(scl) == 0) ;
	pio_set(sda);
	bitDelay();
	while(pio_get(sda) == 0) ;
}

static uint pioWrite(u8 b, const PinConfig* sda, const PinConfig* scl)
{
	for(int i = 0; i < 8; i++) {
		pioWriteBit(b & 0x80, sda, scl);
		b <<= 1;
	}
	return pioReadBit(sda, scl);
}

static u8 pioRead(Bool ack, const PinConfig* sda, const PinConfig* scl)
{
	u8 res = 0;

	for(uint i = 0; i < 8; i++) {
		res <<= 1;
		res |= pioReadBit(sda, scl);
	}

	if(ack)
		pioWriteBit(0, sda, scl);
	else pioWriteBit(1, sda, scl);
	bitDelay();

	return res;
}

static void twiConfigureMaster(AT91S_TWI* twi, uint twck, uint mck)
{
	unsigned int ckdiv = 0;
	unsigned int cldiv;
	unsigned char ok = 0;

	// enable clock for TWI0
	AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_TWI0);

	// SVEN: TWI Slave Mode Enabled
	twi->TWI_CR = AT91C_TWI_SVEN;
	// Reset the TWI
	twi->TWI_CR = AT91C_TWI_SWRST;
	twi->TWI_CR = AT91C_TWI_SWRST;
	twi->TWI_CR = AT91C_TWI_SWRST;
	twi->TWI_RHR;

	// TWI Slave Mode Disabled, TWI Master Mode Disabled
	twi->TWI_CR = AT91C_TWI_SVDIS;
	twi->TWI_CR = AT91C_TWI_MSDIS;

	// Set master mode
	twi->TWI_CR = AT91C_TWI_MSEN;

	// Configure clock
	while(!ok) {
		cldiv = ((mck / (2 * twck)) - 3) / (1 << ckdiv);
		if(cldiv <= 255)
			ok = 1;
		else ckdiv++;
	}

	twi->TWI_CWGR = 0;
	twi->TWI_CWGR = (ckdiv << 16) | (cldiv << 8) | cldiv;
}

static void twiStop(AT91S_TWI* twi)
{
	twi->TWI_CR = AT91C_TWI_STOP;
}

static u8 twiByteReceived(AT91S_TWI* twi)
{
	return (twi->TWI_SR & AT91C_TWI_RXRDY) == AT91C_TWI_RXRDY;
}

static u8 twiReadByte(AT91S_TWI* twi)
{
	return twi->TWI_RHR;
}

static void twiStartRead(AT91S_TWI* twi, u8 address, u32 iaddress, uint isize)
{
	// Set slave address and number of internal address bytes
	twi->TWI_MMR = 0;
	twi->TWI_MMR = (isize << 8) | AT91C_TWI_MREAD | (address << 16);

	// Set internal address bytes
	twi->TWI_IADR = 0;
	twi->TWI_IADR = iaddress;

	// Send START condition
	twi->TWI_CR = AT91C_TWI_START;
}

static u8 twiByteSent(AT91S_TWI* twi)
{
	return ((twi->TWI_SR & AT91C_TWI_TXRDY_MASTER) == AT91C_TWI_TXRDY_MASTER);
}

static void twiWriteByte(AT91S_TWI* twi, u8 byte)
{
	twi->TWI_THR = byte;
}

static void twiStartWrite(AT91S_TWI* twi, u8 address, u32 iaddress, uint isize, u8 byte)
{
	// Set slave address and number of internal address bytes
	twi->TWI_MMR = 0;
	twi->TWI_MMR = (isize << 8) | (address << 16);

	// Set internal address bytes
	twi->TWI_IADR = 0;
	twi->TWI_IADR = iaddress;

	// Write first byte to send
	twiWriteByte(twi, byte);
}

static void twiSendSTOPCondition(AT91S_TWI* twi)
{
	twi->TWI_CR |= AT91C_TWI_STOP;
}

static u8 twiTransferComplete(AT91S_TWI* twi)
{
	return ((twi->TWI_SR & AT91C_TWI_TXCOMP_MASTER) == AT91C_TWI_TXCOMP_MASTER);
}

void twi_configure(void)
{
	pio_configure(twi0PinsTWI, 2);
	twiConfigureMaster(AT91C_BASE_TWI0, 400000, BOARD_MCK);
	pio_configure(twi1PinsPIO, 2);
}

static int twi0_probe(u8 address)
{
	pio_configure(twi0PinsPIO, 2);
	bitDelay();

	pioStart(SDA0, SCL0);
	int res = pioWrite(address << 1, SDA0, SCL0);
	pioStop(SDA0, SCL0);

	pio_configure(twi0PinsTWI, 2);
	twiConfigureMaster(AT91C_BASE_TWI0, 400000, BOARD_MCK);

	if(res)
		return 0;
	else return -1;
}

static int twi0_read(u8 address, u32 iaddress, uint isize, u8* data, uint num)
{
	uint timeout;

	// Set STOP signal if only one byte is sent
	if(num == 1)
		twiStop(AT91C_BASE_TWI0);

	// Start read
	twiStartRead(AT91C_BASE_TWI0, address, iaddress, isize);

	// Read all bytes, setting STOP before the last byte
	while(num > 0) {
		// Last byte
		if(num == 1)
			twiStop(AT91C_BASE_TWI0);
		// Wait for byte then read and store it
		timeout = 0;
		while(!twiByteReceived(AT91C_BASE_TWI0) && (++timeout < TWI_TIMEOUT_MAX))
			;
		if(timeout == TWI_TIMEOUT_MAX)
			return -1;
		*data++ = twiReadByte(AT91C_BASE_TWI0);
		num--;
	}

	// Wait for transfer to be complete
	timeout = 0;
	while(!twiTransferComplete(AT91C_BASE_TWI0) && (++timeout < TWI_TIMEOUT_MAX))
		;
	if(timeout == TWI_TIMEOUT_MAX)
		return -1;

	return num;
}

static int twi0_write(u8 address, u32 iaddress, uint isize, const u8* data, uint num)
{
	uint timeout;

	// Start write
	twiStartWrite(AT91C_BASE_TWI0, address, iaddress, isize, *data++);
	num--;

	// Send all bytes
	while(num > 0) {

		// Wait before sending the next byte
		timeout = 0;
		while(!twiByteSent(AT91C_BASE_TWI0) && (++timeout < TWI_TIMEOUT_MAX))
			;
		if(timeout == TWI_TIMEOUT_MAX)
			return -1;

		twiWriteByte(AT91C_BASE_TWI0, *data++);
		num--;
	}

	// Wait for actual end of transfer
	timeout = 0;

	// Send a STOP condition
	twiSendSTOPCondition(AT91C_BASE_TWI0);

	while(!twiTransferComplete(AT91C_BASE_TWI0) && (++timeout < TWI_TIMEOUT_MAX))
		;
	if(timeout == TWI_TIMEOUT_MAX)
		return -1;

	return num;
}

static int twi1_probe(u8 address)
{
	pioStart(SDA1, SCL1);
	int res = pioWrite(address << 1, SDA1, SCL1);
	pioStop(SDA1, SCL1);
	pioStop(SDA1, SCL1);

	if(res)
		return 0;
	else return -1;
}

static int twi1_write(u8 address, u32 iaddress, uint isize, const u8* data, uint num)
{
	//printf("%02x: %02x %02x\n", address, iaddress, data[0]);

	pioStart(SDA1, SCL1);
	if(pioWrite(address << 1, SDA1, SCL1))
		goto error;
	iaddress <<= (4 - isize) * 8;
	for(int i = 0; i < isize; i++) {
		if(pioWrite((iaddress >> 24) & 0xff, SDA1, SCL1))
			goto error;
		iaddress <<= 8;
	}
	for(int i = 0; i < num; i++) {
		if(pioWrite(data[i], SDA1, SCL1))
			goto error;
	}
	pioStop(SDA1, SCL1);

	return num;

error:
	pioStop(SDA1, SCL1);
	return -1;
}

const TWI g_twi0 = {
	.probe = &twi0_probe,
	.read = &twi0_read,
	.write = &twi0_write
};

const TWI g_twi1 = {
	.probe = &twi1_probe,
	.read = NULL,
	.write = &twi1_write
};
