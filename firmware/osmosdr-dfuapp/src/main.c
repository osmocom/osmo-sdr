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

#include "crt/stdio.h"
#include "driver/dbgio.h"
#include "driver/led.h"
#include "driver/flash.h"
#include "driver/sys.h"
#include "driver/usbdhs.h"
#include "usb/usbdevice.h"
#include "../build/buildid.h"

// USB VDD monitoring pin: PB10

static void displaySN()
{
	char sn[17];

	flash_readUID(sn);
	usbDevice_setSerial(sn);
	printf("-- s/n %s --\n\n", sn);
}

int main(void)
{
	dbgio_configure(115200);

	printf("\n\n** OsmoSDR ** running in DFU mode **\n");
	printf("-- %s %s %s @%s -- %s --\n", BUILDID_DATE, BUILDID_TIME, BUILDID_ZONE, BUILDID_HOST, BUILDID_VCSID);
	printf("-- %s --\n", BUILDID_GCC);
	displaySN();
	printf("Press 'r' for reboot.\n");
	dbgio_flushOutput();

	usbDevice_configure();
	usbdhs_configure();
	usbdhs_connect();

	while(1) {
		for(volatile uint i = 0; i < 1000000; i++) ;
		led_internalSet(1);
		for(volatile uint i = 0; i < 1000000; i++) ;
		led_internalSet(0);

		if(dbgio_isRxReady()) {
			int c;
			while((c = dbgio_getChar()) >= 0) {
				if(c == 'r')
					sys_reset();
			}
			printf("Press 'r' for reboot.\n");
		}

		usbDevice_tick();
	}

	return 0;
}
