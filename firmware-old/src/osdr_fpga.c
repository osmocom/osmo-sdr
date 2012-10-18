
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
#include <stdlib.h>
#include <errno.h>

#include <pio/pio.h>
#include <spi/spi.h>

#include <common.h>
#include <reg_field.h>
#include <osdr_fpga.h>

#define BOARD_OSDR_SPI			AT91C_BASE_SPI0
#define BOARD_OSDR_FPGA_SPI_NPCS	0

#define OSDR_FPGA_SPCK_MAX	1000000

#define OSDR_FPGA_NPCSCONFIG(masterClock) \
		(AT91C_SPI_NCPHA | AT91C_SPI_BITS_8 \
		 | SPI_SCBR(OSDR_FPGA_SPCK_MAX, masterClock) \
		 | SPI_DLYBS(50, masterClock) \
		 | SPI_DLYBCT(0, masterClock))

static const Pin fon_pin = PIN_FON;
static AT91S_SPI *spi = BOARD_OSDR_SPI;

static void write_byte(uint8_t byte)
{
	while ((spi->SPI_SR & AT91C_SPI_TDRE) == 0) {
		//printf("W SR=0x%08x  ", spi->SPI_SR);
		if (spi->SPI_SR & AT91C_SPI_RDRF)
			SPI_Read(BOARD_OSDR_SPI);
	}
	spi->SPI_TDR = byte | SPI_PCS(0);
}


uint32_t osdr_fpga_reg_read(uint8_t reg)
{
	uint32_t val;

	/* make sure that previous transfers have terminated */
	while ((spi->SPI_SR & AT91C_SPI_TXEMPTY) == 0);

	write_byte(0x80 | (reg & 0x7f));
	/* dummy read for command byte transfer */
	SPI_Read(BOARD_OSDR_SPI);

	write_byte(0);
	val = SPI_Read(BOARD_OSDR_SPI) << 24;

	write_byte(0);
	val |= SPI_Read(BOARD_OSDR_SPI) << 16;

	write_byte(0);
	val |= SPI_Read(BOARD_OSDR_SPI) << 8;

	write_byte(0);
	val |= SPI_Read(BOARD_OSDR_SPI);

	return val;
}

void osdr_fpga_reg_write(uint8_t reg, uint32_t val)
{
	/* make sure that previous transfers have terminated */
	while ((spi->SPI_SR & AT91C_SPI_TXEMPTY) == 0);

	write_byte(reg & 0x7f);
	write_byte((val >> 24) & 0xff);
	write_byte((val >> 16) & 0xff);
	write_byte((val >> 8) & 0xff);
	write_byte((val >> 0) & 0xff);

	/* wait until transfer has finished */
	while (!SPI_IsFinished(BOARD_OSDR_SPI));

	/* make sure to flush any received characters */
	while (spi->SPI_SR & AT91C_SPI_RDRF)
		SPI_Read(BOARD_OSDR_SPI);
}

void osdr_fpga_power(int on)
{
	if (on)
		PIO_Set(&fon_pin);
	else
		PIO_Clear(&fon_pin);
}

void osdr_fpga_set_decimation(uint8_t val)
{
	osdr_fpga_reg_write(OSDR_FPGA_REG_DECIMATION, val);
}

void osdr_fpga_set_iq_swap(uint8_t val)
{
	osdr_fpga_reg_write(OSDR_FPGA_REG_IQ_SWAP, (val & 1) ^ 1);
}

void osdr_fpga_set_iq_gain(uint16_t igain, uint16_t qgain)
{
	osdr_fpga_reg_write(OSDR_FPGA_REG_IQ_GAIN, ((uint32_t)igain) | (((uint32_t)qgain) << 16));
}

void osdr_fpga_set_iq_ofs(int16_t iofs, int16_t qofs)
{
	osdr_fpga_reg_write(OSDR_FPGA_REG_IQ_OFS, (((uint32_t)iofs) & 0xffff) | ((((uint32_t)qofs) << 16) & 0xffff0000));
}


/***********************************************************************
 * command integration
 ***********************************************************************/

static int cmd_fpga_dump(struct cmd_state *cs, enum cmd_op op,
			 const char *cmd, int argc, char **argv)
{

	uart_cmd_out(cs, "FPGA ID REG: 0x%08x\n\r", osdr_fpga_reg_read(OSDR_FPGA_REG_ID));
	uart_cmd_out(cs, "FPGA ADC: 0x%08x\n\r", osdr_fpga_reg_read(OSDR_FPGA_REG_ADC_VAL));
	uart_cmd_out(cs, "FPGA PWM1: 0x%08x\n\r", osdr_fpga_reg_read(OSDR_FPGA_REG_PWM1));
	uart_cmd_out(cs, "FPGA ADC TIMING: 0x%08x\n\r", osdr_fpga_reg_read(OSDR_FPGA_REG_ADC_TIMING));

	return 0;
}

static int cmd_fpga_clkdiv(struct cmd_state *cs, enum cmd_op op,
			   const char *cmd, int argc, char **argv)
{
	uint32_t tmp;

	if (argc < 1)
		return -EINVAL;

	tmp = osdr_fpga_reg_read(OSDR_FPGA_REG_ADC_TIMING);
	osdr_fpga_reg_write(OSDR_FPGA_REG_ADC_TIMING,
			    (tmp & 0xFFFF0000) | atoi(argv[0]));

	return 0;
}

enum fpga_field {
	PWM1_DIV, PWM1_DUTY,
	PWM2_DIV, PWM2_DUTY,
	ADC_CLKDIV, ADC_DUTY,
};

const struct reg_field fpga_fields[] = {
	[PWM1_DIV] = { OSDR_FPGA_REG_PWM1, 0, 16 },
	[PWM1_DUTY] = { OSDR_FPGA_REG_PWM1, 16, 16 },
	[PWM2_DIV] = { OSDR_FPGA_REG_PWM2, 0, 16 },
	[PWM2_DUTY] = { OSDR_FPGA_REG_PWM2, 16, 16 },
	[ADC_CLKDIV] = { OSDR_FPGA_REG_ADC_TIMING, 0, 8 },
	[ADC_DUTY] = { OSDR_FPGA_REG_ADC_TIMING, 8, 8},
};

const char *field_names[] = {
	[PWM1_DIV] = 	"fpga.pwm1_div",
	[PWM1_DUTY] =	"fpga.pwm1_duty",
      	[PWM2_DIV] =	"fpga.pwm2_div",
	[PWM2_DUTY] =	"fpga.pwm2_duty",
	[ADC_CLKDIV] =	"fpga.adc_clkdiv",
	[ADC_DUTY] =	"fpga.adc_acqlen",
};

static int fpga_f_write(void *data, uint32_t reg, uint32_t val)
{
	osdr_fpga_reg_write(reg, val);
	return 0;
}

static uint32_t fpga_f_read(void *data, uint32_t reg)
{
	return osdr_fpga_reg_read(reg);
}

static const struct reg_field_ops fpga_fops = {
	.fields		= fpga_fields,
	.field_names	= field_names,
	.num_fields	= ARRAY_SIZE(fpga_fields),
	.data		= NULL,
	.write_cb	= fpga_f_write,
	.read_cb	= fpga_f_read,
};

static int cmd_fpga_field(struct cmd_state *cs, enum cmd_op op,
			  const char *cmd, int argc, char **argv)
{
	return reg_field_cmd(cs, op, cmd, argc, argv, &fpga_fops);
}

static int cmd_fpga_test(struct cmd_state *cs, enum cmd_op op,
			 const char *cmd, int argc, char **argv)
{
	uint32_t on;

	/* in the test mode, the FPGA will not send samples but an incrementing
	 * and decrementing counter to detect lost samples. */
	switch (op) {
	case CMD_OP_SET:
		if (atoi(argv[0]) == 0)
			on = 0;
		else
			on = 1;
		osdr_fpga_reg_write(4, on);
		break;
	case CMD_OP_GET:
		printf("FPGA Test mode is %u\n\r", osdr_fpga_reg_read(4));
		break;
	}
	return 0;
}

static struct cmd cmds[] = {
	{ "fpga.dump", CMD_OP_EXEC, cmd_fpga_dump,
	  "Dump FPGA registers" },
	{ "fpga.pwm1_div", CMD_OP_SET|CMD_OP_GET, cmd_fpga_field,
	  "PWM divider, Freq = 80MHz/(div+1)" },
	{ "fpga.pwm1_duty", CMD_OP_SET|CMD_OP_GET, cmd_fpga_field,
	  "PWM duty cycle" },
	{ "fpga.pwm2_div", CMD_OP_SET|CMD_OP_GET, cmd_fpga_field,
	  "PWM divider, Freq = 80MHz/(div+1)" },
	{ "fpga.pwm2_duty", CMD_OP_SET|CMD_OP_GET, cmd_fpga_field,
	  "PWM duty cycle" },
	{ "fpga.adc_clkdiv", CMD_OP_SET|CMD_OP_GET, cmd_fpga_field,
	  "FPGA Clock Divider for ADC (80 MHz/CLKDIV)" },
	{ "fpga.adc_acqlen", CMD_OP_SET|CMD_OP_GET, cmd_fpga_field,
	  "Num of SCK cycles nCS to AD7357 is held high betewen conversions" },
	{ "fpga.test_mode", CMD_OP_SET|CMD_OP_GET, cmd_fpga_test,
	  "Enable/disable test mode" },
};


/***********************************************************************
 * global init function
 ***********************************************************************/

void osdr_fpga_init(uint32_t masterClock)
{
	SPI_Configure(BOARD_OSDR_SPI, AT91C_ID_SPI0,
			AT91C_SPI_MSTR | AT91C_SPI_PS_FIXED);
	SPI_ConfigureNPCS(BOARD_OSDR_SPI, 0,
			  OSDR_FPGA_NPCSCONFIG(masterClock));
	SPI_Enable(BOARD_OSDR_SPI);

	uart_cmds_register(cmds, ARRAY_SIZE(cmds));
}
