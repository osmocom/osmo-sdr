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

#include "crt/types.h"
#include "at91sam3u4/AT91SAM3U4.h"
#include "board.h"
#include "at91sam3u4/core_cm3.h"
#include "vectors.h"
#include "driver/led.h"

int main(void);

// initialize segments
extern const u32 _sfixed;
extern const u32 _efixed;
extern u32 _srelocate;
extern u32 _erelocate;
extern u32 _szero;
extern u32 _ezero;

extern volatile u32 _dfumode;
#define DFU_MAGIC 0xcd220778

#define AT91C_CKGR_MUL_SHIFT 16
#define AT91C_CKGR_OUT_SHIFT 14
#define AT91C_CKGR_PLLCOUNT_SHIFT 8
#define AT91C_CKGR_DIV_SHIFT 0

#if (BOARD_MCK == 48000000)
// settings at 48/48MHz
#define BOARD_OSCOUNT         (AT91C_CKGR_MOSCXTST & (0x8 << 8))
#define BOARD_PLLR ((1 << 29) | (0x7 << AT91C_CKGR_MUL_SHIFT) \
        | (0x0 << AT91C_CKGR_OUT_SHIFT) |(0x1 << AT91C_CKGR_PLLCOUNT_SHIFT) \
        | (0x1 << AT91C_CKGR_DIV_SHIFT))
#define BOARD_MCKR ( AT91C_PMC_PRES_CLK_2 | AT91C_PMC_CSS_PLLA_CLK)
#elif (BOARD_MCK == 96000000)
// settings at 96/96MHz
#define BOARD_OSCOUNT         (AT91C_CKGR_MOSCXTST & (0x8 << 8))
#define BOARD_PLLR ((1 << 29) | (0x7 << AT91C_CKGR_MUL_SHIFT) \
        | (0x0 << AT91C_CKGR_OUT_SHIFT) |(0x1 << AT91C_CKGR_PLLCOUNT_SHIFT) \
        | (0x1 << AT91C_CKGR_DIV_SHIFT))
#define BOARD_MCKR ( AT91C_PMC_PRES_CLK | AT91C_PMC_CSS_PLLA_CLK)
#else
    #error "no matched BOARD_MCK"
#endif

// define clock timeout
#define CLOCK_TIMEOUT           0xFFFFFFFF

static void startApplication(void)
{
	u32 mode = _dfumode;

	if(mode == DFU_MAGIC) {
		_dfumode = 0;
		return;
	}

	const u32* app = (u32*)(AT91C_IFLASH0 + 16384);
	void (*appReset)(void) = (void(*)(void))app[1];

	if((u32)appReset == 0xffffffff)
		return;
	AT91C_BASE_NVIC->NVIC_VTOFFR = ((u32)(app)) | (0x0 << 7);
	appReset();
}

static void setDefaultMaster(uint enable)
{
	// Set default master
	if(enable == 1) {
		// set default master: SRAM0 -> Cortex-M3 System
		AT91C_BASE_MATRIX->HMATRIX2_SCFG0 |= AT91C_MATRIX_FIXED_DEFMSTR_SCFG0_ARMS | AT91C_MATRIX_DEFMSTR_TYPE_FIXED_DEFMSTR;
		// set default master: SRAM1 -> Cortex-M3 System
		AT91C_BASE_MATRIX->HMATRIX2_SCFG1 |= AT91C_MATRIX_FIXED_DEFMSTR_SCFG1_ARMS | AT91C_MATRIX_DEFMSTR_TYPE_FIXED_DEFMSTR;
		// set default master: internal flash0 -> Cortex-M3 Instruction/Data
		AT91C_BASE_MATRIX->HMATRIX2_SCFG3 |= AT91C_MATRIX_FIXED_DEFMSTR_SCFG3_ARMC | AT91C_MATRIX_DEFMSTR_TYPE_FIXED_DEFMSTR;
	} else {
		// Clear default master: SRAM0 -> Cortex-M3 System
		AT91C_BASE_MATRIX->HMATRIX2_SCFG0 &= (~AT91C_MATRIX_DEFMSTR_TYPE);
		// Clear default master: SRAM1 -> Cortex-M3 System
		AT91C_BASE_MATRIX->HMATRIX2_SCFG1 &= (~AT91C_MATRIX_DEFMSTR_TYPE);
		// Clear default master: Internal flash0 -> Cortex-M3 Instruction/Data
		AT91C_BASE_MATRIX->HMATRIX2_SCFG3 &= (~AT91C_MATRIX_DEFMSTR_TYPE);
	}
}

static void preInit(void)
{
	// switch to 8MHz RC oscillator
	//AT91C_BASE_PMC->PMC_MOR = (0x37 << 16) | AT91C_CKGR_MOSCRCEN | (1 << 4);
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
	AT91C_BASE_DBGU->DBGU_BRGR = 4000000 / (115200 * 16);

	// Configure mode register
	AT91C_BASE_DBGU->DBGU_MR = AT91C_US_PAR_NONE;

	// Disable DMA channel
	AT91C_BASE_DBGU->DBGU_PTCR = AT91C_PDC_RXTDIS | AT91C_PDC_TXTDIS;

	// Enable transmitter
	AT91C_BASE_DBGU->DBGU_CR = AT91C_US_TXEN;
}

static void putCharDirect(u8 c)
{
	while((AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXEMPTY) == 0);
	AT91C_BASE_DBGU->DBGU_THR = c;
}

static void putStrDirect(const char* str)
{
	while(*str != '\0')
		putCharDirect(*str++);
}

static void lowLevelInit(void)
{
	uint timeout;

	// set 2 WS for Embedded Flash Access
	AT91C_BASE_EFC0->EFC_FMR = AT91C_EFC_FWS_2WS;
	AT91C_BASE_EFC1->EFC_FMR = AT91C_EFC_FWS_2WS;

	// disable watchdog
	AT91C_BASE_WDTC->WDTC_WDMR = AT91C_WDTC_WDDIS;

	// initialize main oscillator (if not yet active)
	if(!(AT91C_BASE_PMC->PMC_MOR & AT91C_CKGR_MOSCSEL)) {
		AT91C_BASE_PMC->PMC_MOR = (0x37 << 16) | BOARD_OSCOUNT | AT91C_CKGR_MOSCRCEN | AT91C_CKGR_MOSCXTEN;
		timeout = 0;
		while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MOSCXTS) && (timeout++ < CLOCK_TIMEOUT))
			;
	}

	// switch to 3-20MHz Xtal oscillator
	AT91C_BASE_PMC->PMC_MOR = (0x37 << 16) | BOARD_OSCOUNT | AT91C_CKGR_MOSCRCEN | AT91C_CKGR_MOSCXTEN | AT91C_CKGR_MOSCSEL;
	timeout = 0;
	while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MOSCSELS) && (timeout++ < CLOCK_TIMEOUT))
		;
	AT91C_BASE_PMC->PMC_MCKR = (AT91C_BASE_PMC->PMC_MCKR & ~AT91C_PMC_CSS) | AT91C_PMC_CSS_MAIN_CLK;
	timeout = 0;
	while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY) && (timeout++ < CLOCK_TIMEOUT))
		;

	// initialize PLLA
	AT91C_BASE_PMC->PMC_PLLAR = BOARD_PLLR;
	timeout = 0;
	while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCKA) && (timeout++ < CLOCK_TIMEOUT))
		;

	// initialize UTMI for USB usage
	AT91C_BASE_CKGR->CKGR_UCKR |= (AT91C_CKGR_UPLLCOUNT & (3 << 20)) | AT91C_CKGR_UPLLEN;
	timeout = 0;
	while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCKU) && (timeout++ < CLOCK_TIMEOUT))
		;

	// switch to fast clock
	AT91C_BASE_PMC->PMC_MCKR = (BOARD_MCKR & ~AT91C_PMC_CSS) | AT91C_PMC_CSS_MAIN_CLK;
	timeout = 0;
	while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY) && (timeout++ < CLOCK_TIMEOUT))
		;
	AT91C_BASE_PMC->PMC_MCKR = BOARD_MCKR;
	timeout = 0;
	while(!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY) && (timeout++ < CLOCK_TIMEOUT))
		;

	// Optimize CPU setting for speed, for engineering samples only
	setDefaultMaster(1);

	// Configure baud rate
	AT91C_BASE_DBGU->DBGU_BRGR = BOARD_MCK / (115200 * 16);
}

static void initRAM(void)
{
	const u32* src;
	u32* dest;

	// copy initialized data from flash to RAM
	src = &_efixed;
	dest = &_srelocate;
	if(src != dest) {
		while(dest < &_erelocate)
			*dest++ = *src++;
	}

	// zero fill bss
	for(dest = &_szero; dest < &_ezero; dest++)
		*dest = 0;
}

__attribute__((noreturn)) void resetHandler(void)
{
	for(u32* ptr = (u32*)0x20000000; ptr < (u32*)0x20007ff8; ptr++)
		*ptr = 0;
	for(u32* ptr = (u32*)0x20080000; ptr < (u32*)0x20084000; ptr++)
		*ptr = 0;

	preInit();
	putCharDirect('0');

	// disable all interrupts
	for(int i = 0; i < 8; i++)
		NVIC->ICER[i] = 0xffffffff;

	putStrDirect("preinit...");
	led_configure();
	putCharDirect('1');
	led_internalSet(True);
	putCharDirect('2');
	startApplication();
	putCharDirect('3');
	led_internalSet(False);
	putCharDirect('4');
	lowLevelInit();
	putCharDirect('5');
	initRAM();
	putStrDirect("6...");
	putCharDirect('\r');
	putCharDirect('\n');

	// configure interrupt vector table
	AT91C_BASE_NVIC->NVIC_VTOFFR = ((u32)(&interrupt_table));

	// ready to go...
	__enable_fault_irq();
	__enable_irq();
	main();
	__disable_irq();
	__disable_fault_irq();

	// yeppa...
	while(1)
		;

#if 0
#ifdef BOARD_DFU_BTN_PIOA
	/* There is a PIO button that can be used to enter DFU */
	AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOA;
	AT91C_BASE_PIOA->PIO_PPUER = (1 << BOARD_DFU_BTN_PIOA);
	AT91C_BASE_PIOA->PIO_ODR = (1 << BOARD_DFU_BTN_PIOA);
	AT91C_BASE_PIOA->PIO_PER = (1 << BOARD_DFU_BTN_PIOA);

	if (AT91C_BASE_PIOA->PIO_PDSR & (1 << BOARD_DFU_BTN_PIOA) &&
#else /* BOARD_DFU_BTN_PIOA */
		if (1 &&
#endif /* BOARD_DFU_BTN_PIOA */
			*((unsigned long *)USB_DFU_MAGIC_ADDR) != USB_DFU_MAGIC) {
			BootIntoApp();
		}
#endif
}
