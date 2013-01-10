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
#include "cmd_fpga.h"
#include "driver/sdrfpga.h"
#include "driver/sdrssc.h"
#include "driver/sdrmci.h"
#include "components.h"

static void fpgaPPS(int argc, const char* argv[])
{
	sdrfpga_updatePPS(&g_si570Ctx);
}

static void fpgaStart(int argc, const char* argv[])
{
#if defined(BOARD_SAMPLE_SOURCE_SSC)
	if(sdrssc_dmaStart() >= 0)
		printf("FPGA: SSC started.\n");
	else printf("FPGA: SSC could not be started.\n");
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
#if defined(BOARD_SAMPLE_SOURCE_MCI)
	if(sdrmci_dmaStart() >= 0)
		printf("FPGA: MCI started.\n");
	else printf("FPGA: MCI could not be started.\n");
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)
}

static void fpgaStop(int argc, const char* argv[])
{
#if defined(BOARD_SAMPLE_SOURCE_SSC)
	sdrssc_dmaStop();
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
}

static void fpgaStats(int argc, const char* argv[])
{
#if defined(BOARD_SAMPLE_SOURCE_SSC)
	sdrssc_printStats();
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
#if defined(BOARD_SAMPLE_SOURCE_MCI)
	sdrmci_printStats();
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)
}

static void fpgaFFT(int argc, const char* argv[])
{
#if defined(BOARD_SAMPLE_SOURCE_SSC)
	sdrssc_fft();
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
}

static void fpgaSetReg(int argc, const char* argv[])
{
	if(argc != 2) {
		printf("FPGA: please supply register number and value\n");
		return;
	}
	u32 reg = atoi(argv[0]);
	u32 val = atoi(argv[1]);
	void sdrfpga_regWrite(u8 reg, u32 val);
	sdrfpga_regWrite(reg, val);
}

CONSOLE_CMD_BEGIN(g_fpgaSetRegParameters)
	CONSOLE_CMD_PARAM(PTUnsigned, "register"),
	CONSOLE_CMD_PARAM(PTUnsigned, "value")
CONSOLE_CMD_END()

CONSOLE_GROUP_BEGIN(g_fpgaGroup)
	CONSOLE_GROUP_CMD("pps", "PPS frequency reference", NULL, &fpgaPPS),
	CONSOLE_GROUP_CMD("start", "start sampling", NULL, &fpgaStart),
	CONSOLE_GROUP_CMD("stop", "stop sampling", NULL, &fpgaStop),
	CONSOLE_GROUP_CMD("stats", "print sampling statistics", NULL, &fpgaStats),
	CONSOLE_GROUP_CMD("fft", "calculate and display FFT of input", NULL, &fpgaFFT),
	CONSOLE_GROUP_CMD("sreg", "set FPGA register", g_fpgaSetRegParameters, &fpgaSetReg)
CONSOLE_GROUP_END()

