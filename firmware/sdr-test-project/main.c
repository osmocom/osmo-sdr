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

#include <errno.h>
#include <string.h>
#include <stdlib.h>

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

#include <dmad/dmad.h>
#include <dma/dma.h>

#include <tuner_e4k.h>
#include <si570.h>
#include <osdr_fpga.h>
#include <req_ctx.h>
#include <uart_cmd.h>
#include <fast_source.h>

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
/// Use for power management
#define STATE_IDLE    0
/// The USB device is in suspend state
#define STATE_SUSPEND 4
/// The USB device is in resume state
#define STATE_RESUME  5
unsigned char USBState = STATE_IDLE;

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

/*----------------------------------------------------------------------------
 *         VBus monitoring (optional)
 *----------------------------------------------------------------------------*/

/**  VBus pin instance. */
static const Pin pinVbus = PIN_USB_VBUS;

/**
 *  Handles interrupts coming from PIO controllers.
 */
static void ISR_Vbus(const Pin *pPin)
{
    /* Check current level on VBus */
    if (PIO_Get(&pinVbus))
    {
        TRACE_INFO("VBUS conn\n\r");
        USBD_Connect();
    }
    else
    {
        TRACE_INFO("VBUS discon\n\r");
        USBD_Disconnect();
    }
}

/**
 *  Configures the VBus pin to trigger an interrupt when the level on that pin
 *  changes.
 */
static void VBus_Configure( void )
{
    /* Configure PIO */
    PIO_Configure(&pinVbus, 1);
    PIO_ConfigureIt(&pinVbus, ISR_Vbus);
    PIO_EnableIt(&pinVbus);

    /* Check current level on VBus */
    if (PIO_Get(&pinVbus))
    {
        /* if VBUS present, force the connect */
        USBD_Connect();
    }
    else
    {
        TRACE_INFO("discon\n\r");
        USBD_Disconnect();
    }
}


/*----------------------------------------------------------------------------
 *         Callbacks re-implementation
 *----------------------------------------------------------------------------*/

//------------------------------------------------------------------------------
/// Invoked when the USB device leaves the Suspended state. By default,
/// configures the LEDs.
//------------------------------------------------------------------------------
void USBDCallbacks_Resumed(void)
{
    USBState = STATE_RESUME;
}

//------------------------------------------------------------------------------
/// Invoked when the USB device gets suspended. By default, turns off all LEDs.
//------------------------------------------------------------------------------
void USBDCallbacks_Suspended(void)
{
    USBState = STATE_SUSPEND;
}


static struct cmd_state cmd_state;

static int cmd_tuner_init(struct cmd_state *cs, enum cmd_op op,
			  const char *cmd, int argc, char **argv)
{
	e4k_init(&e4k);
	return 0;
}

static int cmd_tuner_gain(struct cmd_state *cs, enum cmd_op op,
			  const char *cmd, int argc, char **argv)
{
	int i;

	for (i = 0; i < argc; i++)
		e4k_if_gain_set(&e4k, i+1, atoi(argv[i]));

	return 0;
}

static int cmd_rf_freq(struct cmd_state *cs, enum cmd_op op,
		       const char *cmd, int argc, char **argv)
{
	uint32_t freq;

	switch (op) {
	case CMD_OP_SET:
		if (argc < 1)
			return -EINVAL;
		freq = strtoul(argv[0], NULL, 10);
		e4k_tune_freq(&e4k, freq);
		break;
	case CMD_OP_GET:
		freq = e4k.vco.flo;
		uart_cmd_out(cs, "%s:%u\n\r", cmd, freq);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cmd_si570_freq(struct cmd_state *cs, enum cmd_op op,
			  const char *cmd, int argc, char **argv)
{
	uint32_t freq;

	switch (op) {
	case CMD_OP_SET:
		if (argc < 1)
			return -EINVAL;
		freq = strtoul(argv[0], NULL, 10);
		set_si570_freq(freq);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int cmd_si570_dump(struct cmd_state *cs, enum cmd_op op,
			  const char *cmd, int argc, char **argv)
{
	si570_regdump(&si570);
	return 0;
}

static int cmd_ssc_start(struct cmd_state *cs, enum cmd_op op,
			 const char *cmd, int argc, char **argv)
{
	switch (op) {
	case CMD_OP_EXEC:
		ssc_init();
		ssc_dma_start();
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int cmd_ssc_stats(struct cmd_state *cs, enum cmd_op op,
			 const char *cmd, int argc, char **argv)
{
	ssc_stats();
}

static struct cmd cmds[] = {
	{ "tuner.init", CMD_OP_EXEC, cmd_tuner_init,
	  "Initialize the tuner" },
	{ "tuner.freq", CMD_OP_SET|CMD_OP_GET, cmd_rf_freq,
	  "Tune to the specified frequency" },
	{ "tuner.gain", CMD_OP_SET|CMD_OP_GET, cmd_tuner_gain,
	  "Tune to the specified gain" },

	{ "si570.freq", CMD_OP_SET|CMD_OP_GET, cmd_si570_freq,
	  "Change the SI570 clock frequency" },
	{ "si570.dump", CMD_OP_EXEC, cmd_si570_dump,
	  "Dump SI570 registers" },

	{ "ssc.start", CMD_OP_EXEC, cmd_ssc_start,
	  "Start the SSC Receiver" },
	{ "ssc.stats", CMD_OP_EXEC, cmd_ssc_stats,
	  "Statistics about the SSC" },
};

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


	req_ctx_init();
	PIO_InitializeInterrupts(0);

	cmd_state.out = vprintf;
	uart_cmd_reset(&cmd_state);
	uart_cmds_register(cmds, sizeof(cmds)/sizeof(cmds[0]));

	fastsource_init();
	VBus_Configure();

	power_peripherals(1);

	si570_init(&si570, &twid, SI570_I2C_ADDR);
	set_si570_freq(30000000);

	sam3u_e4k_init(&e4k, &twid, E4K_I2C_ADDR);

	osdr_fpga_init(SSC_MCK);
	osdr_fpga_reg_write(OSDR_FPGA_REG_ADC_TIMING, (1 << 8) | 255);
	osdr_fpga_reg_write(OSDR_FPGA_REG_PWM1, (1 << 400) | 800);

	ssc_init();

    // Enter menu loop
    while (1) {

	if (DBGU_IsRxReady()) {
        	key = DBGU_GetChar();
        	// Process user input
		if (uart_cmd_char(&cmd_state, key) == 1) {
			ssc_stats();
		}
	}

	/* Try to (re-)start the SSC DMA if the IN ISO EP is open but the
	 * SSC DMA is not active */
	if (fastsource_interfaces[2] == 1 && !ssc_active())
		ssc_dma_start();
    }
}


