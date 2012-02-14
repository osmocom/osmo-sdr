#ifndef _OSDR_FGPA_H
#define _OSDR_FGPA_H

enum osdr_fpga_reg {
	OSDR_FPGA_REG_ID		= 0,
	OSDR_FPGA_REG_PWM1		= 1,
	OSDR_FPGA_REG_PWM2		= 2,
	OSDR_FPGA_REG_ADC_TIMING	= 3,
	OSDR_FPGA_REG_DUMMY		= 4,
	OSDR_FPGA_REG_ADC_VAL		= 5,
};

void osdr_fpga_power(int on);
void osdr_fpga_init(uint32_t masterClock);
uint32_t osdr_fpga_reg_read(uint8_t reg);
void osdr_fpga_reg_write(uint8_t reg, uint32_t val);

#endif
