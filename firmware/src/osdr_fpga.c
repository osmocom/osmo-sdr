
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

#include <stdint.h>

#include <pio/pio.h>
#include <spi/spi.h>

static const Pin fon_pin = PIN_FON;

#define BOARD_OSDR_SPI			AT91C_BASE_SPI0
#define BOARD_OSDR_FPGA_SPI_NPCS	0

#define OSDR_FPGA_SPCK_MAX	1000000

#define OSDR_FPGA_NPCSCONFIG(masterClock) \
		(AT91C_SPI_NCPHA | AT91C_SPI_BITS_8 \
		 | SPI_SCBR(OSDR_FPGA_SPCK_MAX, masterClock) \
		 | SPI_DLYBS(50, masterClock) \
		 | SPI_DLYBCT(0, masterClock))

void osdr_fpga_power(int on)
{
	if (on)
		PIO_Set(&fon_pin);
	else
		PIO_Clear(&fon_pin);
}

void osdr_fpga_init(uint32_t masterClock)
{
	SPI_Configure(BOARD_OSDR_SPI, AT91C_ID_SPI0,
			AT91C_SPI_MSTR | AT91C_SPI_PS_FIXED);
	SPI_ConfigureNPCS(BOARD_OSDR_SPI, 0,
			  OSDR_FPGA_NPCSCONFIG(masterClock));
	SPI_Enable(BOARD_OSDR_SPI);
}

uint32_t osdr_fpga_reg_read(uint8_t reg)
{
	uint32_t val;

	/* write first byte: command + read */
	SPI_Write(BOARD_OSDR_SPI, 0, 0x80 | (reg & 0x7f));
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	SPI_Read(BOARD_OSDR_SPI);

	/* write four more octets and read back the value */
	SPI_Write(BOARD_OSDR_SPI, 0, 0);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	val = SPI_Read(BOARD_OSDR_SPI) << 24;

	SPI_Write(BOARD_OSDR_SPI, 0, 0);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	val |= SPI_Read(BOARD_OSDR_SPI) << 16;

	SPI_Write(BOARD_OSDR_SPI, 0, 0);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	val |= SPI_Read(BOARD_OSDR_SPI) << 8;

	SPI_Write(BOARD_OSDR_SPI, 0, 0);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	val |= SPI_Read(BOARD_OSDR_SPI);

	return val;
}

void osdr_fpga_reg_write(uint8_t reg, uint32_t val)
{
	/* write first byte: command + read */
	SPI_Write(BOARD_OSDR_SPI, 0, reg & 0x7f);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	SPI_Read(BOARD_OSDR_SPI);

	/* write four more octets and read back the value */
	SPI_Write(BOARD_OSDR_SPI, 0, (val >> 24) & 0xff);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	SPI_Read(BOARD_OSDR_SPI);

	SPI_Write(BOARD_OSDR_SPI, 0, (val >> 16) & 0xff);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	SPI_Read(BOARD_OSDR_SPI);

	SPI_Write(BOARD_OSDR_SPI, 0, (val >> 8) & 0xff);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	SPI_Read(BOARD_OSDR_SPI);

	SPI_Write(BOARD_OSDR_SPI, 0, (val) & 0xff);
	while (!SPI_IsFinished(BOARD_OSDR_SPI));
	SPI_Read(BOARD_OSDR_SPI);
}
