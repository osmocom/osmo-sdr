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
#include "crt/string.h"
#include "cmd_si570.h"
#include "components.h"

static void cmdSI570Status(int argc, const char* argv[])
{
	printf("SI570: Flags: ready: %s, locked: %s.\n", g_si570Ctx.ready ? "yes" : "no", g_si570Ctx.lock ? "yes" : "no");
	printf("SI570: Output frequency: %u kHz.\n", g_si570Ctx.frequency);
}

static void cmdSI570Freq(int argc, const char* argv[])
{
	if(argc == 0) {
		printf("SI570: Output frequency: %u kHz.\n", g_si570Ctx.frequency);
		return;
	}
	u32 freq = atoi(argv[0]);
	if(si570_setFrequency(&g_si570Ctx, freq) >= 0)
		printf("SI570: Output frequency: %u kHz.\n", g_si570Ctx.frequency);
}

CONSOLE_CMD_BEGIN(g_si570TuneParameters)
	CONSOLE_CMD_PARAM(PTUnsigned, "frequency in kHz")
CONSOLE_CMD_END()

CONSOLE_GROUP_BEGIN(g_si570Group)
	CONSOLE_GROUP_CMD("status", "show SI570 status", NULL, &cmdSI570Status),
	CONSOLE_GROUP_CMD("freq", "set frequency in kHz", g_si570TuneParameters, &cmdSI570Freq)
CONSOLE_GROUP_END()

