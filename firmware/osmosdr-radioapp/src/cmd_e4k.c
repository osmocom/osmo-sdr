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
#include "cmd_e4k.h"
#include "driver/e4k.h"
#include "components.h"

static void e4kDump(int argc, const char* argv[])
{
	e4k_dump(&g_e4kCtx);
}

static void e4kTune(int argc, const char* argv[])
{
	if(argc == 0) {
		printf("E4K: locked: %s\n", g_e4kCtx.lock ? "yes" : "no");
		printf("E4K: LO frequency: %u kHz\n", g_e4kCtx.frequency);
		return;
	}
	u32 freq = atoi(argv[0]);
	if(e4k_tune(&g_e4kCtx, freq) >= 0) {
		printf("E4K: LO frequency: %u kHz\n", g_e4kCtx.frequency);
	}
}

CONSOLE_CMD_BEGIN(g_e4kTuneParameters)
	CONSOLE_CMD_PARAM(PTUnsigned, "frequency in kHz")
CONSOLE_CMD_END()

CONSOLE_CMD_BEGIN(g_e4kIQOfsParameters)
	CONSOLE_CMD_PARAM(PTSigned, "I offset"),
	CONSOLE_CMD_PARAM(PTSigned, "Q offset")
CONSOLE_CMD_END()

CONSOLE_GROUP_BEGIN(g_e4kGroup)
	CONSOLE_GROUP_CMD("dump", "dump tuner registers", NULL, &e4kDump),
	CONSOLE_GROUP_CMD("tune", "set frequency in kHz", g_e4kTuneParameters, &e4kTune),
	CONSOLE_GROUP_CMD("iqofs", "set I/Q offset", g_e4kIQOfsParameters, NULL)
CONSOLE_GROUP_END()
