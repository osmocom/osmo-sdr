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
#include "crt/stdio.h"
#include "driver/dbgio.h"
#include "driver/led.h"
#include "driver/twi.h"
#include "driver/flash.h"
#include "driver/usbdhs.h"
#include "driver/sdrfpga.h"
#include "driver/sdrssc.h"
#include "driver/sdrmci.h"
#include "driver/chanfilter.h"
#include "usb/usbdevice.h"
#include "console.h"
#include "components.h"
#include "cmd_sys.h"
#include "cmd_e4k.h"
#include "cmd_fpga.h"
#include "cmd_si570.h"
#include "../build/buildid.h"

#include "driver/pio.h"

// USB VDD monitoring pin: PB10

CONSOLE_BEGIN(g_console)
	CONSOLE_GROUP("si570", "SI570 reference oscillator", g_si570Group),
	CONSOLE_GROUP("e4k", "E4000 tuner", g_e4kGroup),
	CONSOLE_GROUP("fpga", "FPGA", g_fpgaGroup),
	CONSOLE_GROUP("sys", "System", g_sysGroup)
CONSOLE_END()

static const PinConfig g_e4kPowerPins[] = {
	{ AT91C_PIO_PB0, AT91C_BASE_PIOB, AT91C_ID_PIOB, PIO_OUTPUT_1, PIO_DEFAULT }, // RFSTBY
	{ AT91C_PIO_PB1, AT91C_BASE_PIOB, AT91C_ID_PIOB, PIO_OUTPUT_1, PIO_DEFAULT } // PDWN
};

static void readSN()
{
	char sn[17];

	flash_readUID(sn);
	usbDevice_setSerial(sn);
	printf("-- s/n %s --\n", sn);
}

int main(void)
{
	led_configure();
	dbgio_configure(115200);

	puts("\n\n** OsmoSDR ** running in RADIO mode **\n");
	printf("-- %s %s %s @%s -- %s --\n", BUILDID_DATE, BUILDID_TIME, BUILDID_ZONE, BUILDID_HOST, BUILDID_VCSID);
	printf("-- %s --\n", BUILDID_GCC);
	readSN();

	twi_configure();
	si570_configure(&g_si570Ctx, &g_twi0, 0x55);
	si570_setFrequency(&g_si570Ctx, 30000);

	e4k_configure(&g_e4kCtx, &g_twi0, 0x64, si570_getFrequency(&g_si570Ctx), g_e4kPowerPins, 2);
	e4k_tune(&g_e4kCtx, 100000);

	chanfilter_configure(&g_twi1);

	sdrfpga_configure();
#if defined(BOARD_SAMPLE_SOURCE_SSC)
	sdrssc_configure();
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
#if defined(BOARD_SAMPLE_SOURCE_MCI)
	sdrmci_configure();
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)
	usbDevice_configure();
	usbdhs_configure();

	printf("\n");
	dbgio_flushOutput();
	console_configure(g_console);

	usbdhs_connect();

#if defined(BOARD_SAMPLE_SOURCE_SSC)
	sdrssc_dmaStart();
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
#if defined(BOARD_SAMPLE_SOURCE_MCI)
	sdrmci_dmaStart();
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)

	while(1) {
		console_task();
		usbDevice_tick();
	}

	return 0;
}
