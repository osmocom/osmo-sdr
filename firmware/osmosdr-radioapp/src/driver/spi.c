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

#include "spi.h"

void spi_configure(AT91S_SPI* spi, uint id, uint configuration)
{
	AT91C_BASE_PMC->PMC_PCER = 1 << id;
	spi->SPI_IDR = 0xffffffff;
	spi->SPI_CR = AT91C_SPI_SPIDIS;
	// Execute a software reset of the SPI twice
	spi->SPI_CR = AT91C_SPI_SWRST;
	spi->SPI_CR = AT91C_SPI_SWRST;
	spi->SPI_MR = configuration;
}

void spi_configureNPCS(AT91S_SPI* spi, uint npcs, uint configuration)
{
	spi->SPI_CSR[npcs] = configuration;
}

void spi_enable(AT91S_SPI* spi)
{
	spi->SPI_CR = AT91C_SPI_SPIEN;
}

void spi_disable(AT91S_SPI* spi)
{
	spi->SPI_CR = AT91C_SPI_SPIDIS;
}

void spi_write(AT91S_SPI* spi, uint npcs, u16 data)
{
	// Discard contents of RDR register
	//volatile unsigned int discard = spi->SPI_RDR;

	// Send data
	while((spi->SPI_SR & AT91C_SPI_TXEMPTY) == 0)
		;
	spi->SPI_TDR = data | SPI_PCS(npcs);
	while((spi->SPI_SR & AT91C_SPI_TDRE) == 0)
		;
}

u16 spi_read(AT91S_SPI* spi)
{
	while((spi->SPI_SR & AT91C_SPI_RDRF) == 0)
		;
	return spi->SPI_RDR & 0xffff;
}

Bool spi_isFinished(AT91S_SPI* spi)
{
	return ((spi->SPI_SR & AT91C_SPI_TXEMPTY) != 0) ? True : False;
}
