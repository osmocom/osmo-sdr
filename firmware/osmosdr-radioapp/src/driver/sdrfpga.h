#ifndef INCLUDE_SDRFPGA_H
#define INCLUDE_SDRFPGA_H

#include "si570.h"

void sdrfpga_configure(void);
void sdrfpga_setPower(Bool up);
void sdrfpga_regWrite(u8 reg, u32 val);
void sdrfpga_updatePPS(SI570Ctx* si570);
void sdrfpga_setDecimation(uint decimation);
void sdrfpga_setIQSwap(Bool swap);
void sdrfpga_setIQGain(u16 igain, u16 qgain);
void sdrfpga_setIQOfs(s16 iofs, s16 qofs);
void sdrfpga_setTestmode(Bool on);

#endif // INCLUDE_SDRFPGA_H
