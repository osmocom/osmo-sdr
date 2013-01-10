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

#include "mci.h"
#include "../board.h"
#include "../crt/stdio.h"

#define DTOR_1MEGA_CYCLES (AT91C_MCI_DTOCYC | AT91C_MCI_DTOMUL)
#define CSTOR_1MEGA_CYCLES (AT91C_MCI_CSTOCYC | AT91C_MCI_CSTOMUL)
#define MCI_INITIAL_SPEED 48000000

static void dumpSRFlags(u32 sr)
{
	if(sr & AT91C_MCI_CMDRDY)
		printf(" CMDRDY");
	if(sr & AT91C_MCI_RXRDY)
		printf(" RXRDY");
	if(sr & AT91C_MCI_TXRDY)
		printf(" TXRDY");
	if(sr & AT91C_MCI_BLKE)
		printf(" BLKE");
	if(sr & AT91C_MCI_DTIP)
		printf(" DTIP");
	if(sr & AT91C_MCI_NOTBUSY)
		printf(" NOTBUSY");
	if(sr & AT91C_MCI_ENDRX)
		printf(" ENDRX");
	if(sr & AT91C_MCI_ENDTX)
		printf(" ENDTX");
	if(sr & AT91C_MCI_SDIOIRQA)
		printf(" SDIOIRQA");
	if(sr & AT91C_MCI_SDIOIRQB)
		printf(" SDIOIRQB");
	if(sr & AT91C_MCI_SDIOIRQC)
		printf(" SDIOIRQC");
	if(sr & AT91C_MCI_SDIOIRQD)
		printf(" SDIOIRQD");
	if(sr & AT91C_MCI_SDIOWAIT)
		printf(" SDIOWAIT");
	if(sr & AT91C_MCI_CSRCV)
		printf(" CSRCV");
	if(sr & AT91C_MCI_RXBUFF)
		printf(" RXBUFF");
	if(sr & AT91C_MCI_TXBUFE)
		printf(" TXBUFE");
	if(sr & AT91C_MCI_RINDE)
		printf(" RINDE");
	if(sr & AT91C_MCI_RDIRE)
		printf(" RDIRE");
	if(sr & AT91C_MCI_RCRCE)
		printf(" RCRCE");
	if(sr & AT91C_MCI_RENDE)
		printf(" RENDE");
	if(sr & AT91C_MCI_RTOE)
		printf(" RTOE");
	if(sr & AT91C_MCI_DCRCE)
		printf(" DCRCE");
	if(sr & AT91C_MCI_DTOE)
		printf(" DTOE");
	if(sr & AT91C_MCI_CSTOE)
		printf(" CSTOE");
	if(sr & AT91C_MCI_BLKOVRE)
		printf(" BLKOVRE");
	if(sr & AT91C_MCI_DMADONE)
		printf(" DMADONE");
	if(sr & AT91C_MCI_FIFOEMPTY)
		printf(" FIFOEMPTY");
	if(sr & AT91C_MCI_XFRDONE)
		printf(" XFRDONE");
	if(sr & AT91C_MCI_OVRE)
		printf(" OVRE");
	if(sr & AT91C_MCI_UNRE)
		printf(" UNRE");
}

void mci_configure(AT91S_MCI* mci, uint id)
{
	// enable clock for MCI
	AT91C_BASE_PMC->PMC_PCER = (1 << id);

	// reset the MCI
	mci->MCI_CR = AT91C_MCI_SWRST;

	// disable WP
	mci->MCI_WPCR = AT91C_MCI_WP_EN_DISABLE | (0x4d4349 << 8);

	// disable the MCI
	mci->MCI_CR = AT91C_MCI_MCIDIS | AT91C_MCI_PWSDIS;

	// disable all the interrupts
	mci->MCI_IDR = 0xffffffff;

	// set the data timeout register
	mci->MCI_DTOR = DTOR_1MEGA_CYCLES;
	mci->MCI_CSTOR = CSTOR_1MEGA_CYCLES;

	// set the mode register
	uint clkDiv = (BOARD_MCK / (MCI_INITIAL_SPEED * 2)) - 1;
	mci->MCI_MR = (clkDiv | (AT91C_MCI_PWSDIV & (0x7 << 8))) | AT91C_MCI_RDPROOF_ENABLE | (512 << 16);

	// set the SDCard register
	mci->MCI_SDCR = AT91C_MCI_SCDSEL_SLOTA | AT91C_MCI_SCDBUS_4BITS;

	// enable the MCI
	mci->MCI_CR = AT91C_MCI_MCIEN | AT91C_MCI_PWSDIS_1;

	// disable the DMA interface
	mci->MCI_DMA = AT91C_MCI_DMAEN_DISABLE;

	// configure the MCI
	mci->MCI_CFG = AT91C_MCI_FIFOMODE_AMOUNTDATA | AT91C_MCI_FERRCTRL_RWCMD | AT91C_MCI_HSMODE_ENABLE;
}

void mci_setSpeed(AT91S_MCI* mci, u32 decimation)
{
	uint speed = MCI_INITIAL_SPEED;

	/*
	 * Fs = 4MHz
	 * Dec  MCI-Speed
	 * 32    1500000
	 * 16    3000000
	 *  8    6000000
	 *  4   12000000
	 *  2   24000000
	 *  1   48000000
	 */

	if((decimation >= 0) && (decimation <= 7))
		speed = 96000000 / (2 << decimation);

	uint clkDiv = (BOARD_MCK / (speed * 2)) - 1;
	mci->MCI_MR = (clkDiv | (AT91C_MCI_PWSDIV & (0x7 << 8))) | AT91C_MCI_RDPROOF_ENABLE | (512 << 16);
}

void mci_startStream(AT91S_MCI* mci)
{
#if 0
	mci->MCI_ARGR =
		(0 << 31) | // R/W
		(0 << 28) | // Function
		(0 << 27) | // RAW
		(0x111 << 9) | // Register Address
		(0 << 0); // Data
	mci->MCI_CMDR =
		52 |
		AT91C_MCI_RSPTYP_48 |
		AT91C_MCI_SPCMD_NONE |
		AT91C_MCI_OPDCMD_PUSHPULL |
		AT91C_MCI_MAXLAT_64 |
		AT91C_MCI_TRCMD_NO |
		AT91C_MCI_TRDIR_READ |
		AT91C_MCI_IOSPCMD_NONE |
		AT91C_MCI_ATACS_NORMAL;
	while(!(mci->MCI_SR & AT91C_MCI_CMDRDY)) ;
	u32 sr = mci->MCI_SR;
	printf("MCI SR: %08x ", sr);
	dumpSRFlags(sr);
	printf("\n");
	printf("Response %08x %08x %08x %08x\n", mci->MCI_RSPR[0], mci->MCI_RSPR[1], mci->MCI_RSPR[2], mci->MCI_RSPR[3]);
#endif

	mci->MCI_CFG = AT91C_MCI_FIFOMODE_ONEDATA | AT91C_MCI_FERRCTRL_READSR | AT91C_MCI_HSMODE_ENABLE;
	mci->MCI_BLKR = (512 << 16) | (0 << 0);
	mci->MCI_DMA = AT91C_MCI_DMAEN_ENABLE | (1 << 12) | AT91C_MCI_CHKSIZE_4 | 0;

	mci->MCI_ARGR =
		(0 << 31) | // #R/W
		(1 << 28) | // Function
		(1 << 27) | // Block Mode
		(0 << 26) | // Op Code
		(0 << 17) | // Register Address
		(0 << 0); // Block Count
	mci->MCI_CMDR =
		53 |
		AT91C_MCI_RSPTYP_48 |
		AT91C_MCI_SPCMD_NONE |
		AT91C_MCI_OPDCMD_PUSHPULL |
		AT91C_MCI_MAXLAT_64 |
		AT91C_MCI_TRCMD_START |
		AT91C_MCI_TRDIR_READ |
		AT91C_MCI_TRTYP_SDIO_BLOCK |
		AT91C_MCI_IOSPCMD_NONE |
		AT91C_MCI_ATACS_NORMAL;
	//while(!(mci->MCI_SR & AT91C_MCI_CMDRDY)) ;
	/*
	u32 sr = mci->MCI_SR;
	printf("MCI SR: %08x ", sr);
	dumpSRFlags(sr);
	printf("\n");
	printf("Response %08x %08x %08x %08x\n", mci->MCI_RSPR[0], mci->MCI_RSPR[1], mci->MCI_RSPR[2], mci->MCI_RSPR[3]);
	*/
}

void mci_stopStream(AT91S_MCI* mci)
{
	return;
	mci->MCI_ARGR =
		(1 << 31) | // #R/W
		(0 << 28) | // Function
		(0 << 27) | // RAW
		(0x6 << 9) | // Register Address
		(1 << 0); // Data
	mci->MCI_CMDR =
		52 |
		AT91C_MCI_RSPTYP_48 |
		AT91C_MCI_SPCMD_NONE |
		AT91C_MCI_OPDCMD_PUSHPULL |
		AT91C_MCI_MAXLAT_64 |
		AT91C_MCI_TRCMD_NO |
		AT91C_MCI_TRDIR_READ |
		AT91C_MCI_IOSPCMD_NONE |
		AT91C_MCI_ATACS_NORMAL;
	while(!(mci->MCI_SR & AT91C_MCI_CMDRDY)) ;
}

void mci_enableInterrupts(AT91S_MCI* mci, u32 irqs)
{
	mci->MCI_IER = irqs;
}

void mci_disableInterrupts(AT91S_MCI* mci, u32 irqs)
{
	mci->MCI_IDR = irqs;
}
