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
#include "cmd_sys.h"
#include "driver/sys.h"
#include "driver/twi.h"
#include "crt/stdio.h"
#include "crt/string.h"
#include "driver/led.h"
#include "driver/e4k.h"
#include "driver/sdrfpga.h"
#include "components.h"

static void cmdPower(int argc, const char* argv[])
{
	int state = atoi(argv[0]);

	if(argc == 0) {
		printf("Cannot read power state.\n");
		return;
	}

	switch(state) {
		case 0: // off
			sdrfpga_setPower(False);
			e4k_setPower(&g_e4kCtx, False);
			break;

		case 1: // on
			sdrfpga_setPower(True);
			e4k_setPower(&g_e4kCtx, True);
			break;

		default:
			printf("State %d is invalid.\n", state);
			break;
	}

}

static void cmdLED(int argc, const char* argv[])
{
	int led = atoi(argv[0]);
	int state = atoi(argv[1]);

	if(argc == 0) {
		printf("Cannot read LED state.\n");
		return;
	}

	if((state < 0) || (state > 1)) {
		printf("State %d is invalid.\n", state);
		return;
	}
	switch(led) {
		case 0: // internal
			led_internalSet(state);
			break;

		case 1: // external
			led_externalSet(state);
			break;

		default:
			printf("LED %d is unknown.\n", led);
			break;
	}
}

static void cmdI2CScan(int argc, const char* argv[])
{
	int first = 1;

	puts("I2C-Int: ");
	for(uint addr = 0x00; addr < 0x80; addr++) {
		if(g_twi0.probe(addr)) {
			if(first)
				first = 0;
			else puts(" ");
			printf("0x%02x", addr);
		}
	}
	printf("\n");
	first = 1;
	puts("I2C-Ext: ");
	for(uint addr = 0x00; addr < 0x80; addr++) {
		if(g_twi1.probe(addr)) {
			if(first)
				first = 0;
			else puts(" ");
			printf("0x%02x", addr);
		}
	}
	printf("\n");
}

static void cmdSysReboot(int argc, const char* argv[])
{
	sys_reset(False);
}

static void cmdSysDFU(int argc, const char* argv[])
{
	sys_reset(True);
}

CONSOLE_CMD_BEGIN(g_sysPowerParameter)
	CONSOLE_CMD_PARAM(PTUnsigned, "state (0 = off, 1 = on)")
CONSOLE_CMD_END()

CONSOLE_CMD_BEGIN(g_sysLEDParameters)
	CONSOLE_CMD_PARAM(PTUnsigned, "LED id (0 = internal green, 1 = external orange)"),
	CONSOLE_CMD_PARAM(PTUnsigned, "state (0 = off, 1 = on)")
CONSOLE_CMD_END()

CONSOLE_GROUP_BEGIN(g_sysGroup)
	CONSOLE_GROUP_CMD("power", "switch FPGA and E4K on and off", g_sysPowerParameter, &cmdPower),
	CONSOLE_GROUP_CMD("led", "switch LED on and off", g_sysLEDParameters, &cmdLED),
	CONSOLE_GROUP_CMD("i2cscan", "scan I2C bus for connected devices", NULL, &cmdI2CScan),
	CONSOLE_GROUP_CMD("reboot", "reboot OsmoSDR", NULL, &cmdSysReboot),
	CONSOLE_GROUP_CMD("dfu", "switch to Device Firmware Update mode", NULL, &cmdSysDFU)
CONSOLE_GROUP_END()
