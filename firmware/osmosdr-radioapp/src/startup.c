/* most of this code was taken from the at91lib - see LICENSE file for copyright notice */

#include "crt/types.h"
#include "at91sam3u4/AT91SAM3U4.h"
#include "board.h"
#include "at91sam3u4/core_cm3.h"
#include "vectors.h"

int main(void);

// initialize segments
extern const u32 _sfixed;
extern const u32 _efixed;
extern u32 _srelocate;
extern u32 _erelocate;
extern u32 _szero;
extern u32 _ezero;

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
	lowLevelInit();
	initRAM();

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
}
