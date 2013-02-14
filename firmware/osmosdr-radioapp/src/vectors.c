/* most of this code was taken from the at91lib - see LICENSE file for copyright notice */

#include "at91sam3u4/AT91SAM3U4.h"
#include "board.h"
#include "at91sam3u4/core_cm3.h"
#include "vectors.h"
#include "crt/types.h"
#include "crt/stdio.h"
#include "driver/sys.h"

// stack top
extern const u32 _estack;

// dummy handlers - remove from here when used for real
static void nmiHandler(void);
static void hardFaultHandler(void);
static void memManagerHandler(void);
static void busFaultHandler(void);
static void usageFaultHandler(void);
static void svcHandler(void);
static void debugMonHandler(void);
static void pendSVHandler(void);
static void sysTickHandler(void);
static void supcIrqHandler(void);
static void rstcIrqHandler(void);
static void rtcIrqHandler(void);
static void rttIrqHandler(void);
static void wdtIrqHandler(void);
static void pmcIrqHandler(void);
static void efc0IrqHandler(void);
static void efc1IrqHandler(void);
static void hsmc4IrqHandler(void);
static void pioAIrqHandler(void);
static void pioBIrqHandler(void);
static void pioCIrqHandler(void);
static void usart0IrqHandler(void);
static void usart1IrqHandler(void);
static void usart2IrqHandler(void);
static void usart3IrqHandler(void);
static void twi0IrqHandler(void);
static void twi1IrqHandler(void);
static void spi0IrqHandler(void);
static void tc0IrqHandler(void);
static void tc1IrqHandler(void);
static void tc2IrqHandler(void);
static void pwmIrqHandler(void);
static void adc0IrqHandler(void);
static void adc1IrqHandler(void);
static void unusedIrqHandler(void);

__attribute__((section(".vectors"))) InterruptHandler interrupt_table[] = {
	// configure initial stack pointer using linker-generated symbols
	(InterruptHandler)&_estack,
	resetHandler,

	nmiHandler,
	hardFaultHandler,
	memManagerHandler,
	busFaultHandler,
	usageFaultHandler,
	0, 0, 0, 0,             // Reserved
	svcHandler,
	debugMonHandler,
	0,                      // Reserved
	pendSVHandler,
	sysTickHandler,

	// Configurable interrupts
	supcIrqHandler, // 0 SUPPLY CONTROLLER
	rstcIrqHandler, // 1 RESET CONTROLLER
	rtcIrqHandler, // 2 REAL TIME CLOCK
	rttIrqHandler, // 3 REAL TIME TIMER
	wdtIrqHandler, // 4 WATCHDOG TIMER
	pmcIrqHandler, // 5 PMC
	efc0IrqHandler, // 6 EFC0
	efc1IrqHandler, // 7 EFC1
	dbgio_irqHandler, // 8 DBGU
	hsmc4IrqHandler, // 9 HSMC4
	pioAIrqHandler, // 10 Parallel IO Controller A
	pioBIrqHandler, // 11 Parallel IO Controller B
	pioCIrqHandler, // 12 Parallel IO Controller C
	usart0IrqHandler, // 13 USART 0
	usart1IrqHandler, // 14 USART 1
	usart2IrqHandler, // 15 USART 2
	usart3IrqHandler, // 16 USART 3
	mci0_irqHandler, // 17 Multimedia Card Interface
	twi0IrqHandler, // 18 TWI 0
	twi1IrqHandler, // 19 TWI 1
	spi0IrqHandler, // 20 Serial Peripheral Interface
	ssc0_irqHandler, // 21 Serial Synchronous Controller 0
	tc0IrqHandler, // 22 Timer Counter 0
	tc1IrqHandler, // 23 Timer Counter 1
	tc2IrqHandler, // 24 Timer Counter 2
	pwmIrqHandler, // 25 Pulse Width Modulation Controller
	adc0IrqHandler, // 26 ADC controller0
	adc1IrqHandler, // 27 ADC controller1
	hdma_irqHandler, // 28 HDMA
	usbd_irqHandler, // 29 USB Device High Speed UDP_HS
	unusedIrqHandler // 30 not used
};

// Memory Manage Address Register (MMAR) address valid flag
#define CFSR_MMARVALID (0x01 << 7)
// Bus Fault Address Register (BFAR) address valid flag
#define CFSR_BFARVALID (0x01 << 15)

static void panic(const char* haltCause)
{
	dprintf("%s -> Panic.\n\n", haltCause);
	for(int i = 0; i < 20; i++) {
		AT91C_BASE_PIOB->PIO_CODR = AT91C_PIO_PB18;
		AT91C_BASE_PIOB->PIO_CODR = AT91C_PIO_PB19;
		for(int i = 0; i < 750000; i++) __NOP();
		AT91C_BASE_PIOB->PIO_SODR = AT91C_PIO_PB18;
		AT91C_BASE_PIOB->PIO_SODR = AT91C_PIO_PB19;
		for(int i = 0; i < 750000; i++) __NOP();
	}
	sys_reset(True);
}

static void faultReport(const char *pFaultName, const u32* sp)
{
	u32 cfsr;
	u32 mmfar;
	u32 bfar;

	// Report fault
	dprintf("\n*** %s exception ***\n", pFaultName);
	cfsr = SCB->CFSR;
	mmfar = SCB->MMFAR;
	bfar = SCB->BFAR;

	dprintf("HFSR: 0x%08x    SHCSR: 0x%08x\n", SCB->HFSR, SCB->SHCSR);
	dprintf("ICSR: 0x%08x    CFSR:  0x%08x\n", SCB->ICSR, SCB->CFSR);
	dprintf("MMSR: 0x%02x\n", (cfsr & 0xff));
	if((cfsr & CFSR_MMARVALID) != 0)
		dprintf("MMAR: 0x%08x\n", mmfar);
	dprintf("BFSR: 0x%02x\n", ((cfsr >> 8) & 0xff));
	if((cfsr & CFSR_BFARVALID) != 0)
		dprintf("BFAR: 0x%08x\n", bfar);
	dprintf("UFSR: 0x%04x        HFSR: 0x%08x\n", ((cfsr >> 16) & 0xffff), (u32)SCB->HFSR);
	dprintf("DFSR: 0x%08x    AFSR: 0x%08x\n", (u32)SCB->DFSR, (u32)SCB->AFSR);
	//dprintf("  LR: 0x%08x      PC: 0x%08x\n", sp[5], sp[6]);
	dprintf("STACK:\n");
	for(int i = 0; i < 16; i++)
		dprintf("%02x: %08x\n", i, sp[i]);

	// Clear fault status bits (write-clear)
	SCB->CFSR = SCB->CFSR;
	SCB->HFSR = SCB->HFSR;
	SCB->DFSR = SCB->DFSR;
	SCB->AFSR = SCB->AFSR;

	panic("Exception");
}

static void nmiHandler(void)
{
	panic("NMI");
}

void hardFaultHandlerBody(u32* sp)
{
	faultReport("HardFault", sp);
}

static void hardFaultHandler(void)
{
	asm volatile ("tst lr, #0x4");
	asm volatile ("ite eq");
	asm volatile ("mrseq r0, msp");
	asm volatile ("mrsne r0, psp");
	asm volatile ("add r0, r0, #4");
	// because one word has been pushed to sp in this function
	asm volatile ("b hardFaultHandlerBody");
}

void memManagerHandlerBody(u32* sp)
{
	faultReport("MemManager", sp);
}

static void memManagerHandler(void)
{
	asm volatile ("tst lr, #0x4");
	asm volatile ("ite eq");
	asm volatile ("mrseq r0, msp");
	asm volatile ("mrsne r0, psp");
	asm volatile ("add r0, r0, #4");
	// because one word has been pushed to sp in this function
	asm volatile ("b memManagerHandlerBody");
}

void busFaultHandlerBody(u32* sp)
{
	faultReport("BusFault", sp);
}

static void busFaultHandler(void)
{
	asm volatile ("tst lr, #0x4");
	asm volatile ("ite eq");
	asm volatile ("mrseq r0, msp");
	asm volatile ("mrsne r0, psp");
	asm volatile ("add r0, r0, #4");
	// because one word has been pushed to sp in this function
	asm volatile ("b busFaultHandlerBody");
}

void usageFaultHandlerBody(u32* sp)
{
	faultReport("UsageFault", sp);
}

static void usageFaultHandler(void)
{
	asm volatile ("tst lr, #0x4");
	asm volatile ("ite eq");
	asm volatile ("mrseq r0, msp");
	asm volatile ("mrsne r0, psp");
	asm volatile ("add r0, r0, #4");
	// because one word has been pushed to sp in this function
	asm volatile ("b usageFaultHandlerBody");
}

static void svcHandler(void)
{
	panic("SVC");
}

static void debugMonHandler(void)
{
	panic("DebugMon");
}

static void pendSVHandler(void)
{
	panic("PendSV");
}

static void sysTickHandler(void)
{
	panic("SysTick");
}

static void supcIrqHandler(void)
{
	panic("SUPC");
}

static void rstcIrqHandler(void)
{
	panic("RSTC");
}

static void rtcIrqHandler(void)
{
	panic("RTC");
}

static void rttIrqHandler(void)
{
	panic("RTT");
}

static void wdtIrqHandler(void)
{
	panic("WDT");
}

static void pmcIrqHandler(void)
{
	panic("PMC");
}

static void efc0IrqHandler(void)
{
	panic("EFC0");
}

static void efc1IrqHandler(void)
{
	panic("EFC1");
}

static void hsmc4IrqHandler(void)
{
	panic("HSMC4");
}

static void pioAIrqHandler(void)
{
	panic("PIO-A");
}

static void pioBIrqHandler(void)
{
	panic("PIO-B");
}

static void pioCIrqHandler(void)
{
	panic("PIO-C");
}

static void usart0IrqHandler(void)
{
	panic("USART0");
}

static void usart1IrqHandler(void)
{
	panic("USART1");
}

static void usart2IrqHandler(void)
{
	panic("USART2");
}

static void usart3IrqHandler(void)
{
	panic("USART3");
}

static void twi0IrqHandler(void)
{
	panic("TWI0");
}

static void twi1IrqHandler(void)
{
	panic("TWI1");
}

static void spi0IrqHandler(void)
{
	panic("SPI0");
}

static void tc0IrqHandler(void)
{
	panic("TC0");
}

static void tc1IrqHandler(void)
{
	panic("TC1");
}

static void tc2IrqHandler(void)
{
	panic("TC2");
}

static void pwmIrqHandler(void)
{
	panic("PWM");
}

static void adc0IrqHandler(void)
{
	panic("ADC0");
}

static void adc1IrqHandler(void)
{
	panic("ADC1");
}

static void unusedIrqHandler(void)
{
	panic("UNUSED");
}
