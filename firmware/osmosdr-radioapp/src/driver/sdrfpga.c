/* (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
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

#include "sdrfpga.h"
#include "spi.h"
#include "pio.h"
#include "../at91sam3u4/AT91SAM3U4.h"
#include "../board.h"
#include "../crt/stdio.h"

#define OSDR_FPGA_NPCSCONFIG(masterClock) \
	(AT91C_SPI_NCPHA | \
	AT91C_SPI_BITS_8 | \
	SPI_SCBR(OSDR_FPGA_SPCK_MAX, masterClock) | \
	SPI_DLYBS(50, masterClock) | \
	SPI_DLYBCT(0, masterClock))

#define OSDR_FPGA_SPCK_MAX	1000000

#define REG_MAGIC 0x00
#define REG_PWM1 0x01
#define REG_PWM2 0x02
#define REG_ADCMODE 0x03
#define REG_SSCMODE 0x04
#define REG_DECIMATION 0x06
#define REG_IQOFS 0x07
#define REG_IQGAIN 0x08
#define REG_IQSWAP 0x09
#define REG_GPIO_OE 0x0a
#define REG_GPIO_OD 0x0b
#define REG_GPIO_ID 0x0c
#define REG_PPS 0x0d

typedef struct {
	u32 ppsLastUpd;
} OsmoFPGAState;

static OsmoFPGAState g_fpga;

static void writeByte(u8 byte)
{
	while((BOARD_OSDR_SPI->SPI_SR & AT91C_SPI_TDRE) == 0) {
		if(BOARD_OSDR_SPI->SPI_SR & AT91C_SPI_RDRF)
			spi_read(BOARD_OSDR_SPI);
	}
	BOARD_OSDR_SPI->SPI_TDR = byte | SPI_PCS(0);
}

static u32 regRead(u8 reg)
{
	u32 val;

	/* make sure that previous transfers have terminated */
	while((BOARD_OSDR_SPI->SPI_SR & AT91C_SPI_TXEMPTY) == 0)
		;

	writeByte(0x80 | (reg & 0x7f));
	/* dummy read for command byte transfer */
	spi_read(BOARD_OSDR_SPI);

	writeByte(0);
	val = spi_read(BOARD_OSDR_SPI) << 24;

	writeByte(0);
	val |= spi_read(BOARD_OSDR_SPI) << 16;

	writeByte(0);
	val |= spi_read(BOARD_OSDR_SPI) << 8;

	writeByte(0);
	val |= spi_read(BOARD_OSDR_SPI);

	return val;
}

static const PinConfig sdrFPGADataPins[] = {
	{ AT91C_PIO_PA13, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // MISO
	{ AT91C_PIO_PA14, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // MOSI
	{ AT91C_PIO_PA15, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }, // SPCK
	{ AT91C_PIO_PA16, AT91C_BASE_PIOA, AT91C_ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT } // NPCS0
};

static const PinConfig sdrFPGAPowerPin[] = {
	{ AT91C_PIO_PB16, AT91C_BASE_PIOB, AT91C_ID_PIOB, PIO_OUTPUT_1, PIO_DEFAULT } // FON
};

void sdrfpga_configure(void)
{
	pio_configure(sdrFPGADataPins, 4);
	pio_configure(sdrFPGAPowerPin, 1);

	spi_configure(BOARD_OSDR_SPI, AT91C_ID_SPI0, AT91C_SPI_MSTR | AT91C_SPI_PS_FIXED);
	spi_configureNPCS(BOARD_OSDR_SPI, 0, OSDR_FPGA_NPCSCONFIG(BOARD_MCK));
	spi_enable(BOARD_OSDR_SPI);

	u32 magic = regRead(REG_MAGIC);
	if(magic != 0xdeadbeef)
		printf("FPGA magic: 0x%08x - FPGA BROKEN\n", magic);

	// initialize PPS update counter
	g_fpga.ppsLastUpd = (regRead(REG_PPS) >> 25) & 0x7f;
	// set I/Q swap
	sdrfpga_setIQSwap(False);
	// switch off test mode
	sdrfpga_setTestmode(False);
}

void sdrfpga_setPower(Bool up)
{
	if(up)
		pio_set(&sdrFPGAPowerPin[0]);
	else pio_clear(&sdrFPGAPowerPin[0]);
}

void sdrfpga_regWrite(u8 reg, u32 val)
{
	/* make sure that previous transfers have terminated */
	while((BOARD_OSDR_SPI->SPI_SR & AT91C_SPI_TXEMPTY) == 0)
		;

	writeByte(reg & 0x7f);
	writeByte((val >> 24) & 0xff);
	writeByte((val >> 16) & 0xff);
	writeByte((val >> 8) & 0xff);
	writeByte((val >> 0) & 0xff);

	/* wait until transfer has finished */
	while(!spi_isFinished(BOARD_OSDR_SPI))
		;

	/* make sure to flush any received characters */
	while(BOARD_OSDR_SPI->SPI_SR & AT91C_SPI_RDRF)
		spi_read(BOARD_OSDR_SPI);
}

void sdrfpga_updatePPS(SI570Ctx* si570)
{
	u32 pps = regRead(REG_PPS);
	u32 cnt = pps & 0x1ffffff;
	u32 upd = (pps >> 25) & 0x7f;

	if(g_fpga.ppsLastUpd != upd) {
		if(((g_fpga.ppsLastUpd + 1) & 0x7f) == upd)
			si570_ppsUpdate(si570, cnt, False);
		else si570_ppsUpdate(si570, cnt, True);
		g_fpga.ppsLastUpd = upd;
	} else {
		printf("FPGA PPS: No PPS pulse detected since last request.\n");
	}
}

void sdrfpga_setDecimation(uint decimation)
{
	sdrfpga_regWrite(REG_DECIMATION, decimation & 0x07);
}

void sdrfpga_setIQSwap(Bool swap)
{
	// yes, it is swapped in the fpga...
	if(swap)
		sdrfpga_regWrite(REG_IQSWAP, 0);
	else sdrfpga_regWrite(REG_IQSWAP, 1);
}

void sdrfpga_setIQGain(u16 igain, u16 qgain)
{
	sdrfpga_regWrite(REG_IQGAIN, ((u32)igain) | (((u32)qgain) << 16));
}

void sdrfpga_setIQOfs(s16 iofs, s16 qofs)
{
	sdrfpga_regWrite(REG_IQOFS, (((u32)iofs) & 0xffff) | ((((u32)qofs) << 16) & 0xffff0000));
}

void sdrfpga_setTestmode(Bool on)
{
	if(on)
		sdrfpga_regWrite(REG_SSCMODE, regRead(REG_SSCMODE) | 1);
	else sdrfpga_regWrite(REG_SSCMODE, regRead(REG_SSCMODE) & (~1));
}
