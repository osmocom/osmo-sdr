/* ----------------------------------------------------------------------------
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

#include <board.h>
#include <board_memories.h>
#include <pio/pio.h>
#include <irq/irq.h>
#include <twi/twid.h>
#include <twi/twi.h>
#include <dbgu/dbgu.h>
#include <ssc/ssc.h>
#include <utility/assert.h>
#include <utility/math.h>
#include <utility/trace.h>
#include <utility/led.h>

#include <string.h>

#include <dmad/dmad.h>
#include <dma/dma.h>

#include <tuner_e4k.h>
#include <si570.h>
#include <osdr_fpga.h>

#define SSC_MCK    49152000

// TWI clock
#define TWI_CLOCK       100000

// PMC define
#define AT91C_CKGR_PLLR     AT91C_CKGR_PLLAR
#define AT91C_PMC_LOCK      AT91C_PMC_LOCKA

#define AT91C_CKGR_MUL_SHIFT         16
#define AT91C_CKGR_OUT_SHIFT         14
#define AT91C_CKGR_PLLCOUNT_SHIFT     8
#define AT91C_CKGR_DIV_SHIFT          0

#define E4K_I2C_ADDR 		0x64
#define SI570_I2C_ADDR 		0x55

//------------------------------------------------------------------------------
//         Local variables
//------------------------------------------------------------------------------

/// List of pins to configure.
static const Pin pins[] = {PINS_TWI0, PIN_PCK0, PINS_LEDS, PINS_SPI0,
			   PINS_MISC, PINS_SSC, PINS_FPGA_JTAG};

static Twid twid;
static struct e4k_state e4k;
static struct si570_ctx si570;

static void set_si570_freq(uint32_t freq)
{
	si570_set_freq(&si570, freq/1000, 0);
	e4k.vco.fosc = freq;
}

static void power_peripherals(int on)
{
	if (on) {
		osdr_fpga_power(1);
		sam3u_e4k_power(&e4k, 1);
		sam3u_e4k_stby(&e4k, 0);
	} else {
		osdr_fpga_power(0);
		sam3u_e4k_stby(&e4k, 1);
		sam3u_e4k_power(&e4k, 0);
	}
}

struct reg {
	unsigned int offset;
	const char *name;
};

struct reg dma_regs[] = {
	{ 0,	"GCFG" },
	{ 4, 	"EN" },
	{ 8,	"SREQ" },
	{ 0xC,	"CREQ" },
	{ 0x10,	"LAST" },
	{ 0x20, "EBCIMR" },
	{ 0x24, "EBCISR" },
	{ 0x30, "CHSR" },
	{ 0,	NULL}
};

struct reg dma_ch_regs[] = {
	{ 0,	"SADDR" },
	{ 4,	"DADDR" },
	{ 8,	"DSCR" },
	{ 0xC,	"CTRLA" },
	{ 0x10, "CTRLB" },
	{ 0x14, "CFG" },
	{ 0,	NULL}
};

void reg_dump(struct reg *regs, uint32_t *base)
{
	struct reg *r;
	for (r = regs; r->offset || r->name; r++) {
		uint32_t *addr = ((uint8_t *)base + r->offset);
		printf("%s\t%08x:\t%08x\n\r", r->name, addr, *addr);
	}
}

static void dma_dump_regs(void)
{
	reg_dump(dma_regs, AT91C_BASE_HDMA);
	reg_dump(dma_ch_regs, (uint8_t *)AT91C_BASE_HDMA_CH_0 + (BOARD_SSC_DMA_CHANNEL*0x28));
}


static void ssc_irq_hdlr(void)
{
	AT91S_SSC *ssc = AT91C_BASE_SSC0;
	uint8_t status = ssc->SSC_SR;

}

static DmaLinkList	LLI_CH[MAX_SSC_LLI_SIZE];
static int dma_complete = 0;
static const uint32_t dummy = 0xdeadbeef;

static void ssc_dma_single(void *dest, unsigned int len)
{
	LED_Set(0);
	dma_complete = 0;

	memset(dest, 0, len);

	/* clear any pending interrupts */
	DMA_DisableChannel(BOARD_SSC_DMA_CHANNEL);
	DMA_GetStatus();
	DMA_SetSourceBufferMode(BOARD_SSC_DMA_CHANNEL, DMA_TRANSFER_SINGLE,
				(AT91C_HDMA_SRC_ADDRESS_MODE_FIXED >> 24));
	DMA_SetSourceBufferSize(BOARD_SSC_DMA_CHANNEL,
#if 0
				len>>2 | AT91C_HDMA_SCSIZE_4 | AT91C_HDMA_DCSIZE_4,
				AT91C_HDMA_SRC_WIDTH_WORD >> 24,
				AT91C_HDMA_DST_WIDTH_WORD >> 28, 0);
#else
				len>>2 | AT91C_HDMA_SCSIZE_1 | AT91C_HDMA_DCSIZE_1,
				AT91C_HDMA_SRC_WIDTH_BYTE >> 24,
				AT91C_HDMA_DST_WIDTH_BYTE >> 28, 0);
#endif
	DMA_SetDestBufferMode(BOARD_SSC_DMA_CHANNEL, DMA_TRANSFER_SINGLE,
				(AT91C_HDMA_DST_ADDRESS_MODE_INCR >> 28));
	DMA_SetFlowControl(BOARD_SSC_DMA_CHANNEL, AT91C_HDMA_FC_PER2MEM >> 21);
	DMA_SetConfiguration(BOARD_SSC_DMA_CHANNEL, BOARD_SSC_DMA_HW_SRC_REQ_ID
				| BOARD_SSC_DMA_HW_DEST_REQ_ID
				| AT91C_HDMA_SRC_H2SEL_HW
				| AT91C_HDMA_DST_H2SEL_HW
				| AT91C_HDMA_SOD_DISABLE
				| AT91C_HDMA_FIFOCFG_LARGESTBURST);
	DMA_SetSourceAddr(BOARD_SSC_DMA_CHANNEL, &AT91C_BASE_SSC0->SSC_RHR);
	//DMA_SetSourceAddr(BOARD_SSC_DMA_CHANNEL, &dummy);
	DMA_SetDestinationAddr(BOARD_SSC_DMA_CHANNEL, (unsigned int) dest);
	dma_dump_regs();
	DMA_EnableChannel(BOARD_SSC_DMA_CHANNEL);

	printf("Initialized Single DMA (len=%u)\n\r", len);
}
#define DMA_CTRLA	(AT91C_HDMA_SRC_WIDTH_WORD|AT91C_HDMA_DST_WIDTH_WORD|AT91C_HDMA_SCSIZE_4|AT91C_HDMA_DCSIZE_4)
#define DMA_CTRLB 	(AT91C_HDMA_DST_DSCR_FETCH_FROM_MEM |	\
			 AT91C_HDMA_DST_ADDRESS_MODE_INCR |	\
			 AT91C_HDMA_SRC_DSCR_FETCH_DISABLE |	\
			 AT91C_HDMA_SRC_ADDRESS_MODE_FIXED |	\
			 AT91C_HDMA_FC_PER2MEM)

static void ssc_dma_llc(void *dest, unsigned int len)
{
	LED_Set(0);
	dma_complete = 0;

	memset(dest, 0, len);

	LLI_CH[0].sourceAddress = &AT91C_BASE_SSC0->SSC_RHR;
	//LLI_CH[0].sourceAddress = &dummy;
	LLI_CH[0].destAddress = (unsigned int) dest;
	LLI_CH[0].controlA = len/4 | DMA_CTRLA;
	LLI_CH[0].controlB = DMA_CTRLB;
	LLI_CH[0].descriptor = 0;

	/* clear any pending interrupts */
	DMA_DisableChannel(BOARD_SSC_DMA_CHANNEL);
	DMA_GetStatus();
	DMA_SetDescriptorAddr(BOARD_SSC_DMA_CHANNEL, (unsigned int)&LLI_CH[0]);
	DMA_SetSourceAddr(BOARD_SSC_DMA_CHANNEL, &AT91C_BASE_SSC0->SSC_RHR);
	DMA_SetSourceBufferMode(BOARD_SSC_DMA_CHANNEL, DMA_TRANSFER_LLI,
				(AT91C_HDMA_SRC_ADDRESS_MODE_FIXED >> 24));
	DMA_SetDestBufferMode(BOARD_SSC_DMA_CHANNEL, DMA_TRANSFER_LLI,
				(AT91C_HDMA_DST_ADDRESS_MODE_INCR >> 28));

	DMA_SetFlowControl(BOARD_SSC_DMA_CHANNEL, AT91C_HDMA_FC_PER2MEM >> 21);
	DMA_SetConfiguration(BOARD_SSC_DMA_CHANNEL,
				BOARD_SSC_DMA_HW_SRC_REQ_ID | BOARD_SSC_DMA_HW_DEST_REQ_ID
				| AT91C_HDMA_SRC_H2SEL_HW \
				| AT91C_HDMA_DST_H2SEL_SW \
				| AT91C_HDMA_SOD_DISABLE \
				| AT91C_HDMA_FIFOCFG_LARGESTBURST);

	dma_dump_regs();
	printf("enabling channel...\n\r");
	DMA_EnableChannel(BOARD_SSC_DMA_CHANNEL);

	printf("Initialized LLC DMA (len=%u)\n\r", len);
}


#define BTC(N)	(1 << N)
#define CBTC(N)	(1 << 8+N)
#define	ERR(N)	(1 << 16+N)

void HDMA_IrqHandler(void)
{
	unsigned int status = DMA_GetStatus();
	LED_Clear(0);

	if (status & BTC(BOARD_SSC_DMA_CHANNEL)) {
		dma_complete = 1;
		LED_Clear(0);
	}
}

static int ssc_init(void)
{
	SSC_DisableReceiver(AT91C_BASE_SSC0);
	SSC_Configure(AT91C_BASE_SSC0, AT91C_ID_SSC0, 0, BOARD_MCK);
	SSC_ConfigureReceiver(AT91C_BASE_SSC0, AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE |
					 AT91C_SSC_CKG_NONE | AT91C_SSC_START_FALL_RF |
					 AT91C_SSC_CKI,
					 AT91C_SSC_MSBF | 32-1 );
#if 0
	IRQ_ConfigureIT(AT91C_ID_SSC0, 0, ssc_irq_hdlr);
	IRQ_EnableIT(AT91C_ID_SSC0);

	//SSC_EnableInterrupts(AT91C_BASE_SSC0, AT91C_SSC_RXRDY | AT91C_SSC_OVRUN);
#endif
	SSC_EnableReceiver(AT91C_BASE_SSC0);

	/* Enable DMA controller and register interrupt handler */
	PMC_EnablePeripheral(AT91C_ID_HDMA);
	DMA_Enable();
	IRQ_ConfigureIT(AT91C_ID_HDMA, 0, HDMA_IrqHandler);
	IRQ_EnableIT(AT91C_ID_HDMA);
	DMA_EnableIt(BTC(BOARD_SSC_DMA_CHANNEL) | CBTC(BOARD_SSC_DMA_CHANNEL) |
		     ERR(BOARD_SSC_DMA_CHANNEL));

	printf("initialzied SSC\n\r");
}

static void DisplayMenu(void)
{
	printf("Menu:\r\n"
		"[0] fpga+rf power on\r\n"
		"[1] si570 init\r\n"
		"[2] e4k init\r\n"
		"[f] si570 30MHz freq\r\n"
		"[r] si570 regdump\r\n"
		"[q] 100 MHz\r\n"
		"[w] 101 MHz\r\n"
		"[p] FPGA ID reg\r\n"
		"[a] FPGA ADC reg\r\n"
		"[s] SSC init\r\n"
		"[S] SSC single DMA xfer\r\n"
		"\r\n"
		);
}

static uint32_t dma_buf[1024/4];

//------------------------------------------------------------------------------
/// Main function
//------------------------------------------------------------------------------
int main(void)
{
    unsigned char key;
    unsigned char isValid;
    static int freq = 800;

    // Configure all pins
    PIO_Configure(pins, PIO_LISTSIZE(pins));

    LED_Configure(0);
    LED_Set(0);
    LED_Configure(1);
    LED_Set(1);

    // Initialize the DBGU
    TRACE_CONFIGURE(DBGU_STANDARD, 115200, BOARD_MCK);

    printf("trace configured!!\n");

    // Switch to Main clock
    AT91C_BASE_PMC->PMC_MCKR = (AT91C_BASE_PMC->PMC_MCKR & ~AT91C_PMC_CSS) | AT91C_PMC_CSS_MAIN_CLK;
    while ((AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY) == 0);

    // Configure PLL to 98.285MHz
    *AT91C_CKGR_PLLR = ((1 << 29) | (171 << AT91C_CKGR_MUL_SHIFT) \
        | (0x0 << AT91C_CKGR_OUT_SHIFT) |(0x3f << AT91C_CKGR_PLLCOUNT_SHIFT) \
        | (21 << AT91C_CKGR_DIV_SHIFT));
    while ((AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCK) == 0);

    // Configure master clock in two operations
    AT91C_BASE_PMC->PMC_MCKR = (( AT91C_PMC_PRES_CLK_2 | AT91C_PMC_CSS_PLLA_CLK) & ~AT91C_PMC_CSS) | AT91C_PMC_CSS_MAIN_CLK;
    while ((AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY) == 0);
    AT91C_BASE_PMC->PMC_MCKR = ( AT91C_PMC_PRES_CLK_2 | AT91C_PMC_CSS_PLLA_CLK);
    while ((AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY) == 0);

    // DBGU reconfiguration
    DBGU_Configure(DBGU_STANDARD, 115200, SSC_MCK);

    // Configure and enable the TWI (required for accessing the DAC)
    *AT91C_PMC_PCER = (1<< AT91C_ID_TWI0); 
    TWI_ConfigureMaster(AT91C_BASE_TWI0, TWI_CLOCK, SSC_MCK);
    TWID_Initialize(&twid, AT91C_BASE_TWI0);

    printf("-- osmo-sdr testing project %s --\n\r", SOFTPACK_VERSION);
    printf("-- %s\n\r", BOARD_NAME);
    printf("-- Compiled: %s %s --\n\r", __DATE__, __TIME__);

	power_peripherals(1);
	si570_init(&si570, &twid, SI570_I2C_ADDR);
	set_si570_freq(30000000);
	osdr_fpga_init(SSC_MCK);
	osdr_fpga_reg_write(OSDR_FPGA_REG_ADC_TIMING, (1 << 8) | 255);
	osdr_fpga_reg_write(OSDR_FPGA_REG_PWM1, (1 << 400) | 800);

    // Enter menu loop
    while (1) {

        // Display menu
        DisplayMenu();

        // Process user input
        key = DBGU_GetChar();
 	//key = 0;

	switch (key) {
	case '0':
		power_peripherals(1);
		break;
	case '1':
		si570_init(&si570, &twid, SI570_I2C_ADDR);
		break;
	case '2':
		sam3u_e4k_init(&e4k, &twid, E4K_I2C_ADDR);
		e4k_init(&e4k);
		break;
	case 'f':
		set_si570_freq(30000000);
		break;
	case 'r':
		si570_regdump(&si570);
		break;
	case 'q':
		e4k_tune_freq(&e4k, 100000000);
		break;
	case 'w':
		e4k_tune_freq(&e4k, 101000000);
		break;
	case 'p':
		{
			uint32_t reg;
			osdr_fpga_power(1);
			osdr_fpga_init(SSC_MCK);
			reg = osdr_fpga_reg_read(OSDR_FPGA_REG_ID);
			printf("FPGA ID REG: 0x%08x\n\r", reg);
			osdr_fpga_reg_write(OSDR_FPGA_REG_ADC_TIMING, (1 << 8) | 255);
		}
		break;
	case 'a':
		printf("FPGA ADC: 0x%08x\n\r", osdr_fpga_reg_read(OSDR_FPGA_REG_ADC_VAL));
		printf("FPGA PWM1: 0x%08x\n\r", osdr_fpga_reg_read(OSDR_FPGA_REG_PWM1));
		printf("FPGA ADC TIMING: 0x%08x\n\r", osdr_fpga_reg_read(OSDR_FPGA_REG_ADC_TIMING));
		break;
	case '+':
		freq += 100;
		osdr_fpga_reg_write(OSDR_FPGA_REG_PWM1, ((freq/2) << 16) | freq);
		break;
	case '-':
		freq -= 100;
		osdr_fpga_reg_write(OSDR_FPGA_REG_PWM1, ((freq/2) << 16) | freq);
		break;
	case 's':
		ssc_init();
		break;
	case 'S':
		SSC_DisableReceiver(AT91C_BASE_SSC0);
		//ssc_dma_single(dma_buf, sizeof(dma_buf));
		ssc_dma_llc(dma_buf, sizeof(dma_buf));
		SSC_EnableReceiver(AT91C_BASE_SSC0);
		break;
	}
	//osdr_fpga_reg_write(OSDR_FPGA_REG_PWM1, ((freq/2) << 16) | freq);
	printf("\t\t\tcur_ssc_data = 0x%08x\n\r", AT91C_BASE_SSC0->SSC_RHR);
	{
		int i;
		for (i = 0; i < sizeof(dma_buf)/sizeof(dma_buf[0]); i++) {
			if (i == 0 || dma_buf[i] != dma_buf[i-1])
				printf("\t\t\tdma_ssc_data[%u] = 0x%08x\n\r", i, dma_buf[i]);
			//break;
		}
	}
	dma_dump_regs();
	if (dma_complete) {
		printf("=======> DMA complete\n\r");
		dma_complete = 0;
	}
    }
}

