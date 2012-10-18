#ifndef INCLUDE_CHANFILTER_H
#define INCLUDE_CHANFILTER_H

#include "twi.h"

void chanfilter_configure(const TWI* twi);
int chanfilter_set(uint poti, u8 value);
int chanfilter_setAll(u8 p0v0, u8 p0v1, u8 p1v0, u8 p1v1);
int chanfilter_regWrite(uint chip, u8 reg, u8 value);

#endif // INCLUDE_CHANFILTER_H
