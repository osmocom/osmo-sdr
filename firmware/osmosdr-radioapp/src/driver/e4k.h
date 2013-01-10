#ifndef INCLUDE_E4K_H
#define INCLUDE_E4K_H

#include "../crt/types.h"
#include "twi.h"
#include "pio.h"

enum e4k_band {
	E4K_BAND_VHF2 = 0,
	E4K_BAND_VHF3 = 1,
	E4K_BAND_UHF = 2,
	E4K_BAND_L = 3
};

enum e4k_if_filter {
	E4K_IF_FILTER_MIX,
	E4K_IF_FILTER_CHAN,
	E4K_IF_FILTER_RC
};

enum e4k_agc_mode {
	E4K_AGC_MOD_SERIAL = 0x0,
	E4K_AGC_MOD_IF_PWM_LNA_SERIAL = 0x1,
	E4K_AGC_MOD_IF_PWM_LNA_AUTONL = 0x2,
	E4K_AGC_MOD_IF_PWM_LNA_SUPERV = 0x3,
	E4K_AGC_MOD_IF_SERIAL_LNA_PWM = 0x4,
	E4K_AGC_MOD_IF_PWM_LNA_PWM = 0x5,
	E4K_AGC_MOD_IF_DIG_LNA_SERIAL = 0x6,
	E4K_AGC_MOD_IF_DIG_LNA_AUTON = 0x7,
	E4K_AGC_MOD_IF_DIG_LNA_SUPERV = 0x8,
	E4K_AGC_MOD_IF_SERIAL_LNA_AUTON = 0x9,
	E4K_AGC_MOD_IF_SERIAL_LNA_SUPERV = 0xa,
};


struct e4k_pll_params {
	u32 fosc;
	u32 intended_flo;
	u32 flo;
	u16 x;
	u8 z;
	u8 r;
	u8 r_idx;
	u8 threephase;
};

typedef struct {
	const TWI* twi;
	u8 i2c_addr;
	enum e4k_band band;
	struct e4k_pll_params vco;

	const PinConfig* powerPins;
	uint numPowerPins;

	u32 frequency;
	Bool lock;
} E4KCtx;

void e4k_configure(E4KCtx* ctx, const TWI* twi, u8 addr, u32 refFreq, const PinConfig* powerPins, uint numPowerPins);
void e4k_setPower(E4KCtx* ctx, Bool up);
void e4k_dump(E4KCtx* e4k);
int e4k_setReg(E4KCtx* e4k, u8 reg, u8 val);
int e4k_tune(E4KCtx* e4k, u32 freq);
int e4k_reInit(E4KCtx* e4k);
int e4k_ifGainSet(E4KCtx* e4k, u8 stage, s8 value);
int e4k_mixerGainSet(E4KCtx* e4k, s8 value);
int e4k_commonModeSet(E4KCtx* e4k, s8 value);
int e4k_ifFilterBWSet(E4KCtx* e4k, enum e4k_if_filter filter, u32 bandwidth);
int e4k_ifFilterChanEnable(E4KCtx* e4k, Bool on);
int e4k_manualDCOffsetSet(E4KCtx* e4k, s8 iofs, s8 irange, s8 qofs, s8 qrange);
int e4k_dcOffsetCalibrate(E4KCtx* e4k);
int e4k_dcOffsetGenTable(E4KCtx* e4k);
int e4k_setLNAGain(E4KCtx* e4k, int32_t gain);
int e4k_enableManualGain(E4KCtx* e4k, u8 manual);
int e4k_setEnhGain(E4KCtx* e4k, s32 gain);

#endif // INCLUDE_E4K_H
