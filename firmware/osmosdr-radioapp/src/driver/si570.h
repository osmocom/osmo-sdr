#ifndef INCLUDE_SI570_H
#define INCLUDE_SI570_H

#include "twi.h"

typedef struct {
	u32 min;
	u32 max;
} SI570Range;

typedef struct {
	SI570Range freqRange[3];
	u32 initFreq;
} SI570Info;

typedef struct {
	const TWI* twi;
	u8 addr;

	u32 frequency;
	int trim;

	u32 xtal;
	const SI570Info* info;

	Bool ready;
	Bool lock;

	uint ppsValid;
	u32 ppsCnt;
} SI570Ctx;

void si570_configure(SI570Ctx* ctx, const TWI* twi, u8 addr);
int si570_init(SI570Ctx* ctx);
int si570_setFrequency(SI570Ctx* ctx, u32 freq);
int si570_setFrequencyAndTrim(SI570Ctx* ctx, u32 freq, u32 trim);
u32 si570_getFrequency(SI570Ctx* ctx);
Bool si570_isLocked(SI570Ctx* ctx);
void si570_ppsUpdate(SI570Ctx* ctx, u32 cnt, Bool reset);
int si570_regWrite(SI570Ctx* ctx, u8 reg, uint len, const u8* data);

#endif // INCLUDE_SI570_H
