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
		"\r\n"
		);
}

//------------------------------------------------------------------------------
/// Main function
//------------------------------------------------------------------------------
int main(void)
{
    unsigned char key;
    unsigned char isValid;

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

    // Enter menu loop
    while (1) {

        // Display menu
        DisplayMenu();

        // Process user input
        key = DBGU_GetChar();

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
		}
		break;
	}
    }
}

