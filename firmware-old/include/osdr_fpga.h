#ifndef _OSDR_FGPA_H
#define _OSDR_FGPA_H

enum osdr_fpga_reg {
	OSDR_FPGA_REG_ID		= 0,
	OSDR_FPGA_REG_PWM1		= 1,
	OSDR_FPGA_REG_PWM2		= 2,
	OSDR_FPGA_REG_ADC_TIMING	= 3,
	OSDR_FPGA_REG_DUMMY		= 4,
	OSDR_FPGA_REG_ADC_VAL		= 5,
	OSDR_FPGA_REG_DECIMATION = 6,
	OSDR_FPGA_REG_IQ_OFS = 7,
	OSDR_FPGA_REG_IQ_GAIN = 8,
	OSDR_FPGA_REG_IQ_SWAP = 9,
};

void osdr_fpga_power(int on);
void osdr_fpga_init(uint32_t masterClock);
uint32_t osdr_fpga_reg_read(uint8_t reg);
void osdr_fpga_reg_write(uint8_t reg, uint32_t val);
void osdr_fpga_set_decimation(uint8_t val);
void osdr_fpga_set_iq_swap(uint8_t val);
void osdr_fpga_set_iq_gain(uint16_t igain, uint16_t qgain);
void osdr_fpga_set_iq_ofs(int16_t iofs, int16_t qofs);

#endif
