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

#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include "../crt/stdio.h"

#include "reg_field.h"
#include "e4k.h"
#include "pio.h"
// #include <tuner_e4k.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* If this is defined, the limits are somewhat relaxed compared to what the
 * vendor claims is possible */
#define OUT_OF_SPEC

#define MHZ(x)	((x)*1000*1000)
#define KHZ(x)	((x)*1000)

enum e4k_reg {
	E4K_REG_MASTER1 = 0x00,
	E4K_REG_MASTER2 = 0x01,
	E4K_REG_MASTER3 = 0x02,
	E4K_REG_MASTER4 = 0x03,
	E4K_REG_MASTER5 = 0x04,
	E4K_REG_CLK_INP = 0x05,
	E4K_REG_REF_CLK = 0x06,
	E4K_REG_SYNTH1 = 0x07,
	E4K_REG_SYNTH2 = 0x08,
	E4K_REG_SYNTH3 = 0x09,
	E4K_REG_SYNTH4 = 0x0a,
	E4K_REG_SYNTH5 = 0x0b,
	E4K_REG_SYNTH6 = 0x0c,
	E4K_REG_SYNTH7 = 0x0d,
	E4K_REG_SYNTH8 = 0x0e,
	E4K_REG_SYNTH9 = 0x0f,
	E4K_REG_FILT1 = 0x10,
	E4K_REG_FILT2 = 0x11,
	E4K_REG_FILT3 = 0x12,
	// gap
	E4K_REG_GAIN1 = 0x14,
	E4K_REG_GAIN2 = 0x15,
	E4K_REG_GAIN3 = 0x16,
	E4K_REG_GAIN4 = 0x17,
	// gap
	E4K_REG_AGC1 = 0x1a,
	E4K_REG_AGC2 = 0x1b,
	E4K_REG_AGC3 = 0x1c,
	E4K_REG_AGC4 = 0x1d,
	E4K_REG_AGC5 = 0x1e,
	E4K_REG_AGC6 = 0x1f,
	E4K_REG_AGC7 = 0x20,
	E4K_REG_AGC8 = 0x21,
	// gap
	E4K_REG_AGC11 = 0x24,
	E4K_REG_AGC12 = 0x25,
	// gap
	E4K_REG_DC1 = 0x29,
	E4K_REG_DC2 = 0x2a,
	E4K_REG_DC3 = 0x2b,
	E4K_REG_DC4 = 0x2c,
	E4K_REG_DC5 = 0x2d,
	E4K_REG_DC6 = 0x2e,
	E4K_REG_DC7 = 0x2f,
	E4K_REG_DC8 = 0x30,
	// gap
	E4K_REG_QLUT0 = 0x50,
	E4K_REG_QLUT1 = 0x51,
	E4K_REG_QLUT2 = 0x52,
	E4K_REG_QLUT3 = 0x53,
	// gap
	E4K_REG_ILUT0 = 0x60,
	E4K_REG_ILUT1 = 0x61,
	E4K_REG_ILUT2 = 0x62,
	E4K_REG_ILUT3 = 0x63,
	// gap
	E4K_REG_DCTIME1 = 0x70,
	E4K_REG_DCTIME2 = 0x71,
	E4K_REG_DCTIME3 = 0x72,
	E4K_REG_DCTIME4 = 0x73,
	E4K_REG_PWM1 = 0x74,
	E4K_REG_PWM2 = 0x75,
	E4K_REG_PWM3 = 0x76,
	E4K_REG_PWM4 = 0x77,
	E4K_REG_BIAS = 0x78,
	E4K_REG_CLKOUT_PWDN = 0x7a,
	E4K_REG_CHFILT_CALIB = 0x7b,
	E4K_REG_I2C_REG_ADDR = 0x7d,
// FIXME
};

#define E4K_MASTER1_RESET       (1 << 0)
#define E4K_MASTER1_NORM_STBY   (1 << 1)
#define E4K_MASTER1_POR_DET     (1 << 2)

#define E4K_SYNTH1_PLL_LOCK     (1 << 0)
#define E4K_SYNTH1_BAND_SHIF    1

#define E4K_SYNTH7_3PHASE_EN    (1 << 3)

#define E4K_SYNTH8_VCOCAL_UPD   (1 << 2)

#define E4K_FILT3_DISABLE       (1 << 5)

#define E4K_AGC1_LIN_MODE       (1 << 4)
#define E4K_AGC1_LNA_UPDATE     (1 << 5)
#define E4K_AGC1_LNA_G_LOW      (1 << 6)
#define E4K_AGC1_LNA_G_HIGH     (1 << 7)

#define E4K_AGC6_LNA_CAL_REQ    (1 << 4)

#define E4K_AGC7_MIX_GAIN_AUTO  (1 << 0)
#define E4K_AGC7_GAIN_STEP_5dB  (1 << 5)

#define E4K_AGC8_SENS_LIN_AUTO  (1 << 0)

#define E4K_AGC11_LNA_GAIN_ENH  (1 << 0)

#define E4K_DC1_CAL_REQ         (1 << 0)

#define E4K_DC5_I_LUT_EN        (1 << 0)
#define E4K_DC5_Q_LUT_EN        (1 << 1)
#define E4K_DC5_RANGE_DET_EN    (1 << 2)
#define E4K_DC5_RANGE_EN        (1 << 3)
#define E4K_DC5_TIMEVAR_EN      (1 << 4)

#define E4K_CLKOUT_DISABLE      0x96

#define E4K_CHFCALIB_CMD        (1 << 0)

#define E4K_AGC1_MOD_MASK       0xF

uint32_t unsigned_delta(uint32_t a, uint32_t b)
{
	if (a > b)
		return a - b;
	else
		return b - a;
}

/* look-up table bit-width -> mask */
static const uint8_t width2mask[] = {
	0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff
};

/***********************************************************************
 * Register Access */

#if 1
/*! \brief Write a register of the tuner chip
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \param[in] val value to be written
 *  \returns 0 on success, negative in case of error
 */
static int e4k_reg_write(E4KCtx* e4k, uint8_t reg, uint8_t val)
{
	if(e4k->twi->write(e4k->i2c_addr, reg, 1, &val, 1) < 0) {
		printf("E4K: I2C write error\n");
		return -1;
	}
	return 0;
}

/*! \brief Read a register of the tuner chip
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \returns positive 8bit register contents on success, negative in case of error
 */
static int e4k_reg_read(E4KCtx* e4k, uint8_t reg)
{
	uint8_t val;

	if(e4k->twi->read(e4k->i2c_addr, reg, 1, &val, 1) < 0) {
		printf("E4K: I2C read error\n");
		return -1;
	}

	return val;
}
#endif

/*! \brief Set or clear some (masked) bits inside a register
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \param[in] mask bit-mask of the value
 *  \param[in] val data value to be written to register
 *  \returns 0 on success, negative in case of error
 */
static int e4k_reg_set_mask(E4KCtx* e4k, uint8_t reg,
		     uint8_t mask, uint8_t val)
{
	uint8_t tmp = e4k_reg_read(e4k, reg);

	if ((tmp & mask) == val)
		return 0;

	return e4k_reg_write(e4k, reg, (tmp & ~mask) | (val & mask));
}

/*! \brief Write a given field inside a register
 *  \param[in] e4k reference to the tuner
 *  \param[in] field structure describing the field
 *  \param[in] val value to be written
 *  \returns 0 on success, negative in case of error
 */
static int e4k_field_write(E4KCtx* e4k, const struct reg_field *field, uint8_t val)
{
	int rc;
	uint8_t mask;

	rc = e4k_reg_read(e4k, field->reg);
	if (rc < 0)
		return rc;

	mask = width2mask[field->width] << field->shift;

	return e4k_reg_set_mask(e4k, field->reg, mask, val << field->shift);
}

/*! \brief Read a given field inside a register
 *  \param[in] e4k reference to the tuner
 *  \param[in] field structure describing the field
 *  \returns positive value of the field, negative in case of error
 */
static int e4k_field_read(E4KCtx* e4k, const struct reg_field *field)
{
	int rc;

	rc = e4k_reg_read(e4k, field->reg);
	if (rc < 0)
		return rc;

	rc = (rc >> field->shift) & width2mask[field->width];

	return rc;
}

/***********************************************************************
 * Filter Control */

static const uint32_t rf_filt_center_uhf[] = {
	MHZ(360), MHZ(380), MHZ(405), MHZ(425),
	MHZ(450), MHZ(475), MHZ(505), MHZ(540),
	MHZ(575), MHZ(615), MHZ(670), MHZ(720),
	MHZ(760), MHZ(840), MHZ(890), MHZ(970)
};

static const uint32_t rf_filt_center_l[] = {
	MHZ(1300), MHZ(1320), MHZ(1360), MHZ(1410),
	MHZ(1445), MHZ(1460), MHZ(1490), MHZ(1530),
	MHZ(1560), MHZ(1590), MHZ(1640), MHZ(1660),
	MHZ(1680), MHZ(1700), MHZ(1720), MHZ(1750)
};

static int closest_arr_idx(const uint32_t *arr, unsigned int arr_size, uint32_t freq)
{
	unsigned int i, bi = 0;
	uint32_t best_delta = 0xffffffff;

	/* iterate over the array containing a list of the center
	 * frequencies, selecting the closest one */
	for (i = 0; i < arr_size; i++) {
		uint32_t delta = unsigned_delta(freq, arr[i]);
		if (delta < best_delta) {
			best_delta = delta;
			bi = i;
		}
	}

	return bi;
}

/* return 4-bit index as to which RF filter to select */
static int choose_rf_filter(enum e4k_band band, uint32_t freq)
{
	int rc;

	switch (band) {
		case E4K_BAND_VHF2:
		case E4K_BAND_VHF3:
			rc = 0;
			break;
		case E4K_BAND_UHF:
			rc = closest_arr_idx(rf_filt_center_uhf,
						 ARRAY_SIZE(rf_filt_center_uhf),
						 freq);
			break;
		case E4K_BAND_L:
			rc = closest_arr_idx(rf_filt_center_l,
						 ARRAY_SIZE(rf_filt_center_l),
						 freq);
			break;
		default:
			rc = -EINVAL;
			break;
	}

	return rc;
}

/* \brief Automatically select apropriate RF filter based on e4k state */
int e4k_rf_filter_set(E4KCtx* e4k)
{
	int rc;

	rc = choose_rf_filter(e4k->band, e4k->vco.flo);
	if (rc < 0)
		return rc;

	return e4k_reg_set_mask(e4k, E4K_REG_FILT1, 0xF, rc);
}

/* Mixer Filter */
static const uint32_t mix_filter_bw[] = {
	KHZ(27000), KHZ(27000), KHZ(27000), KHZ(27000),
	KHZ(27000), KHZ(27000), KHZ(27000), KHZ(27000),
	KHZ(4600), KHZ(4200), KHZ(3800), KHZ(3400),
	KHZ(3300), KHZ(2700), KHZ(2300), KHZ(1900)
};

/* IF RC Filter */
static const uint32_t ifrc_filter_bw[] = {
	KHZ(21400), KHZ(21000), KHZ(17600), KHZ(14700),
	KHZ(12400), KHZ(10600), KHZ(9000), KHZ(7700),
	KHZ(6400), KHZ(5300), KHZ(4400), KHZ(3400),
	KHZ(2600), KHZ(1800), KHZ(1200), KHZ(1000)
};

/* IF Channel Filter */
static const uint32_t ifch_filter_bw[] = {
	KHZ(5500), KHZ(5300), KHZ(5000), KHZ(4800),
	KHZ(4600), KHZ(4400), KHZ(4300), KHZ(4100),
	KHZ(3900), KHZ(3800), KHZ(3700), KHZ(3600),
	KHZ(3400), KHZ(3300), KHZ(3200), KHZ(3100),
	KHZ(3000), KHZ(2950), KHZ(2900), KHZ(2800),
	KHZ(2750), KHZ(2700), KHZ(2600), KHZ(2550),
	KHZ(2500), KHZ(2450), KHZ(2400), KHZ(2300),
	KHZ(2280), KHZ(2240), KHZ(2200), KHZ(2150)
};

static const uint32_t *if_filter_bw[] = {
	mix_filter_bw,
	ifch_filter_bw,
	ifrc_filter_bw,
};

static const uint32_t if_filter_bw_len[] = {
	ARRAY_SIZE(mix_filter_bw),
	ARRAY_SIZE(ifch_filter_bw),
	ARRAY_SIZE(ifrc_filter_bw),
};

static const struct reg_field if_filter_fields[] = {
	{
		E4K_REG_FILT2, 4, 4,
	},
	{
		E4K_REG_FILT3, 0, 5,
	},
	{
		E4K_REG_FILT2, 0, 4,
	}
};

static int find_if_bw(enum e4k_if_filter filter, uint32_t bw)
{
	if (filter >= ARRAY_SIZE(if_filter_bw))
		return -EINVAL;

	return closest_arr_idx(if_filter_bw[filter],
			       if_filter_bw_len[filter], bw);
}

/*! \brief Set the filter band-width of any of the IF filters
 *  \param[in] e4k reference to the tuner chip
 *  \param[in] filter filter to be configured
 *  \param[in] bandwidth bandwidth to be configured
 *  \returns positive actual filter band-width, negative in case of error
 */
int e4k_ifFilterBWSet(E4KCtx* e4k, enum e4k_if_filter filter, u32 bandwidth)
{
	int bw_idx;
	const struct reg_field *field;

	if (filter >= ARRAY_SIZE(if_filter_bw))
		return -EINVAL;

	bw_idx = find_if_bw(filter, bandwidth);

	field = &if_filter_fields[filter];

	return e4k_field_write(e4k, field, bw_idx);
}

/*! \brief Enables / Disables the channel filter
 *  \param[in] e4k reference to the tuner chip
 *  \param[in] on 1=filter enabled, 0=filter disabled
 *  \returns 0 success, negative errors
 */
int e4k_ifFilterChanEnable(E4KCtx* e4k, Bool on)
{
	return e4k_reg_set_mask(e4k, E4K_REG_FILT3, E4K_FILT3_DISABLE, on ? 0 : E4K_FILT3_DISABLE);
}

static int e4k_if_filter_bw_get(E4KCtx* e4k, enum e4k_if_filter filter)
{
	const uint32_t *arr;
	int rc;
	const struct reg_field *field;

	if (filter >= ARRAY_SIZE(if_filter_bw))
		return -EINVAL;

	field = &if_filter_fields[filter];

	rc = e4k_field_read(e4k, field);
	if (rc < 0)
		return rc;

	arr = if_filter_bw[filter];

	return arr[rc];
}


/***********************************************************************
 * Frequency Control */

#define E4K_FVCO_MIN_KHZ	2600000	/* 2.6 GHz */
#define E4K_FVCO_MAX_KHZ	3900000	/* 3.9 GHz */
#define E4K_PLL_Y		65536

#ifdef OUT_OF_SPEC
#define E4K_FLO_MIN_MHZ		50
#define E4K_FLO_MAX_MHZ		2200UL
#else
#define E4K_FLO_MIN_MHZ		64
#define E4K_FLO_MAX_MHZ		1700
#endif

struct pll_settings {
	uint32_t freq;
	uint8_t reg_synth7;
	uint8_t mult;
};

static const struct pll_settings pll_vars[] = {
	{KHZ(72400),	(1 << 3) | 7,	48},
	{KHZ(81200),	(1 << 3) | 6,	40},
	{KHZ(108300),	(1 << 3) | 5,	32},
	{KHZ(162500),	(1 << 3) | 4,	24},
	{KHZ(216600),	(1 << 3) | 3,	16},
	{KHZ(325000),	(1 << 3) | 2,	12},
	{KHZ(350000),	(1 << 3) | 1,	8},
	{KHZ(432000),	(0 << 3) | 3,	8},
	{KHZ(667000),	(0 << 3) | 2,	6},
	{KHZ(1200000),	(0 << 3) | 1,	4}
};

static int is_fvco_valid(uint32_t fvco_z)
{
	/* check if the resulting fosc is valid */
	if (fvco_z/1000 < E4K_FVCO_MIN_KHZ ||
	    fvco_z/1000 > E4K_FVCO_MAX_KHZ) {
		printf("Fvco %u invalid\n\r", fvco_z);
		return 0;
	}

	return 1;
}

static int is_fosc_valid(uint32_t fosc)
{
	if (fosc < MHZ(16) || fosc > MHZ(30)) {
		printf("Fosc %u invalid\n\r", fosc);
		return 0;
	}

	return 1;
}

static int is_z_valid(uint32_t z)
{
	if (z > 255) {
		printf("Z %u invalid\n\r", z);
		return 0;
	}

	return 1;
}

/*! \brief Determine if 3-phase mixing shall be used or not */
static int use_3ph_mixing(uint32_t flo)
{
	/* this is a magic number somewhre between VHF and UHF */
	if (flo < MHZ(350))
		return 1;

	return 0;
}

/* \brief compute Fvco based on Fosc, Z and X
 * \returns positive value (Fvco in Hz), 0 in case of error */
static uint64_t compute_fvco(uint32_t f_osc, uint8_t z, uint16_t x)
{
	uint64_t fvco_z, fvco_x, fvco;

	/* We use the following transformation in order to
	 * handle the fractional part with integer arithmetic:
	 *  Fvco = Fosc * (Z + X/Y) <=> Fvco = Fosc * Z + (Fosc * X)/Y
	 * This avoids X/Y = 0.  However, then we would overflow a 32bit
	 * integer, as we cannot hold e.g. 26 MHz * 65536 either.
	 */
	fvco_z = (uint64_t)f_osc * z;

#if 0
	if (!is_fvco_valid(fvco_z))
		return 0;
#endif

	fvco_x = ((uint64_t)f_osc * x) / E4K_PLL_Y;

	fvco = fvco_z + fvco_x;

	return fvco;
}

static uint32_t compute_flo(uint32_t f_osc, uint8_t z, uint16_t x, uint8_t r)
{
	uint64_t fvco = compute_fvco(f_osc, z, x);
	if (fvco == 0)
		return -EINVAL;

	return fvco / r;
}

static int e4k_band_set(E4KCtx* e4k, enum e4k_band band)
{
	int rc;

	switch (band) {
	case E4K_BAND_VHF2:
	case E4K_BAND_VHF3:
	case E4K_BAND_UHF:
		e4k_reg_write(e4k, E4K_REG_BIAS, 3);
		break;
	case E4K_BAND_L:
		e4k_reg_write(e4k, E4K_REG_BIAS, 0);
		break;
	}
	/* workaround: if we don't reset this register before writing to it,
	 * we get a gap between 325-350 MHz */
	rc = e4k_reg_set_mask(e4k, E4K_REG_SYNTH1, 0x06, 0);
	rc = e4k_reg_set_mask(e4k, E4K_REG_SYNTH1, 0x06, band << 1);
	if (rc >= 0)
		e4k->band = band;

	return rc;
}

/*! \brief Compute PLL parameters for givent target frequency
 *  \param[out] oscp Oscillator parameters, if computation successful
 *  \param[in] fosc Clock input frequency applied to the chip (Hz)
 *  \param[in] intended_flo target tuning frequency (Hz)
 *  \returns actual PLL frequency, as close as possible to intended_flo,
 *	     0 in case of error
 */
static uint32_t e4k_compute_pll_params(struct e4k_pll_params *oscp, uint32_t fosc, uint32_t intended_flo)
{
	uint32_t i;
	uint8_t r = 2;
	uint64_t intended_fvco, remainder;
	uint64_t z = 0;
	uint32_t x;
	int flo;
	int three_phase_mixing = 0;
	oscp->r_idx = 0;

	if (!is_fosc_valid(fosc))
		return 0;

	for(i = 0; i < ARRAY_SIZE(pll_vars); ++i) {
		if(intended_flo < pll_vars[i].freq) {
			three_phase_mixing = (pll_vars[i].reg_synth7 & 0x08) ? 1 : 0;
			oscp->r_idx = pll_vars[i].reg_synth7;
			r = pll_vars[i].mult;
			break;
		}
	}

	//printf("Fint=%u, R=%u\n\r", intended_flo, r);

	/* flo(max) = 1700MHz, R(max) = 48, we need 64bit! */
	intended_fvco = (uint64_t)intended_flo * r;

	/* compute integral component of multiplier */
	z = intended_fvco / fosc;

	/* compute fractional part.  this will not overflow,
	* as fosc(max) = 30MHz and z(max) = 255 */
	remainder = intended_fvco - (fosc * z);
	/* remainder(max) = 30MHz, E4K_PLL_Y = 65536 -> 64bit! */
	x = (remainder * E4K_PLL_Y) / fosc;
	/* x(max) as result of this computation is 65536 */

	flo = compute_flo(fosc, z, x, r);

	oscp->fosc = fosc;
	oscp->flo = flo;
	oscp->intended_flo = intended_flo;
	oscp->r = r;
//	oscp->r_idx = pll_vars[i].reg_synth7 & 0x0;
	oscp->threephase = three_phase_mixing;
	oscp->x = x;
	oscp->z = z;

	return flo;
}

static int e4k_tune_params(E4KCtx* e4k, struct e4k_pll_params *p)
{
	/* program R + 3phase/2phase */
	e4k_reg_write(e4k, E4K_REG_SYNTH7, p->r_idx);
	/* program Z */
	e4k_reg_write(e4k, E4K_REG_SYNTH3, p->z);
	/* program X */
	e4k_reg_write(e4k, E4K_REG_SYNTH4, p->x & 0xff);
	e4k_reg_write(e4k, E4K_REG_SYNTH5, p->x >> 8);

	/* we're in auto calibration mode, so there's no need to trigger it */

	memcpy(&e4k->vco, p, sizeof(e4k->vco));

	/* set the band */
	if (e4k->vco.flo < MHZ(140))
		e4k_band_set(e4k, E4K_BAND_VHF2);
	else if (e4k->vco.flo < MHZ(350))
		e4k_band_set(e4k, E4K_BAND_VHF3);
	else if (e4k->vco.flo < MHZ(1135))
		e4k_band_set(e4k, E4K_BAND_UHF);
	else
		e4k_band_set(e4k, E4K_BAND_L);

	/* select and set proper RF filter */
	e4k_rf_filter_set(e4k);

	return e4k->vco.flo;
}

/***********************************************************************
 * Gain Control */

static const int8_t if_stage1_gain[] = {
	-3, 6
};

static const int8_t if_stage23_gain[] = {
	0, 3, 6, 9
};

static const int8_t if_stage4_gain[] = {
	0, 1, 2, 2
};

static const int8_t if_stage56_gain[] = {
	3, 6, 9, 12, 15, 15, 15, 15
};

static const int8_t *if_stage_gain[] = {
	0,
	if_stage1_gain,
	if_stage23_gain,
	if_stage23_gain,
	if_stage4_gain,
	if_stage56_gain,
	if_stage56_gain
};

static const uint8_t if_stage_gain_len[] = {
	0,
	ARRAY_SIZE(if_stage1_gain),
	ARRAY_SIZE(if_stage23_gain),
	ARRAY_SIZE(if_stage23_gain),
	ARRAY_SIZE(if_stage4_gain),
	ARRAY_SIZE(if_stage56_gain),
	ARRAY_SIZE(if_stage56_gain)
};

static const struct reg_field if_stage_gain_regs[] = {
	{ 0, 0, 0 },
	{ E4K_REG_GAIN3, 0, 1 },
	{ E4K_REG_GAIN3, 1, 2 },
	{ E4K_REG_GAIN3, 3, 2 },
	{ E4K_REG_GAIN3, 5, 2 },
	{ E4K_REG_GAIN4, 0, 3 },
	{ E4K_REG_GAIN4, 3, 3 }
};

static const int32_t lnagain[] = {
	-50,	0,
	-25,	1,
	0,	4,
	25,	5,
	50,	6,
	75,	7,
	100,	8,
	125,	9,
	150,	10,
	175,	11,
	200,	12,
	250,	13,
	300,	14,
};

static const int32_t enhgain[] = {
	10, 30, 50, 70
};

int e4k_setLNAGain(E4KCtx* e4k, int32_t gain)
{
	uint32_t i;
	for(i = 0; i < ARRAY_SIZE(lnagain)/2; ++i) {
		if(lnagain[i*2] == gain) {
			e4k_reg_set_mask(e4k, E4K_REG_GAIN1, 0xf, lnagain[i*2+1]);
			return gain;
		}
	}
	return -EINVAL;
}

int e4k_setEnhGain(E4KCtx* e4k, s32 gain)
{
	uint32_t i;
	for(i = 0; i < ARRAY_SIZE(enhgain); ++i) {
		if(enhgain[i] == gain) {
			e4k_reg_set_mask(e4k, E4K_REG_AGC11, 0x7, E4K_AGC11_LNA_GAIN_ENH | (i << 1));
			return gain;
		}
	}
	e4k_reg_set_mask(e4k, E4K_REG_AGC11, 0x7, 0);

	/* special case: 0 = off*/
	if(0 == gain)
		return 0;
	else
		return -EINVAL;
}

int e4k_enableManualGain(E4KCtx* e4k, u8 manual)
{
	if (manual) {
		/* Set LNA mode to manual */
		e4k_reg_set_mask(e4k, E4K_REG_AGC1, E4K_AGC1_MOD_MASK, E4K_AGC_MOD_SERIAL);

		/* Set Mixer Gain Control to manual */
		e4k_reg_set_mask(e4k, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO, 0);
	} else {
		/* Set LNA mode to auto */
		e4k_reg_set_mask(e4k, E4K_REG_AGC1, E4K_AGC1_MOD_MASK, E4K_AGC_MOD_IF_SERIAL_LNA_AUTON);
		/* Set Mixer Gain Control to auto */
		e4k_reg_set_mask(e4k, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO, 1);

		e4k_reg_set_mask(e4k, E4K_REG_AGC11, 0x7, 0);
	}

	return 0;
}

static int find_stage_gain(uint8_t stage, int8_t val)
{
	const int8_t *arr;
	int i;

	if (stage >= ARRAY_SIZE(if_stage_gain))
		return -EINVAL;

	arr = if_stage_gain[stage];

	for (i = 0; i < if_stage_gain_len[stage]; i++) {
		if (arr[i] == val)
			return i;
	}
	return -EINVAL;
}

/*! \brief Set the gain of one of the IF gain stages
 *  \param[e4k] handle to the tuner chip
 *  \param [stage] numbere of the stage (1..6)
 *  \param [value] gain value in dBm
 *  \returns 0 on success, negative in case of error
 */
int e4k_ifGainSet(E4KCtx* e4k, u8 stage, s8 value)
{
	int rc;
	uint8_t mask;
	const struct reg_field *field;

	rc = find_stage_gain(stage, value);
	if (rc < 0)
		return rc;

	/* compute the bit-mask for the given gain field */
	field = &if_stage_gain_regs[stage];
	mask = width2mask[field->width] << field->shift;

	return e4k_reg_set_mask(e4k, field->reg, mask, rc << field->shift);
}

int e4k_mixerGainSet(E4KCtx* e4k, s8 value)
{
	uint8_t bit;

	switch (value) {
	case 4:
		bit = 0;
		break;
	case 12:
		bit = 1;
		break;
	default:
		return -EINVAL;
	}

	return e4k_reg_set_mask(e4k, E4K_REG_GAIN2, 1, bit);
}

int e4k_commonModeSet(E4KCtx* e4k, s8 value)
{
	if(value < 0)
		return -EINVAL;
	else if(value > 7)
		return -EINVAL;

	return e4k_reg_set_mask(e4k, E4K_REG_DC7, 7, value);
}

/***********************************************************************
 * DC Offset */

int e4k_manualDCOffsetSet(E4KCtx* e4k, s8 iofs, s8 irange, s8 qofs, s8 qrange)
{
	int res;

	if((iofs < 0x00) || (iofs > 0x3f))
		return -EINVAL;
	if((irange < 0x00) || (irange > 0x03))
		return -EINVAL;
	if((qofs < 0x00) || (qofs > 0x3f))
		return -EINVAL;
	if((qrange < 0x00) || (qrange > 0x03))
		return -EINVAL;

	res = e4k_reg_set_mask(e4k, E4K_REG_DC2, 0x3f, iofs);
	if(res < 0)
		return res;

	res = e4k_reg_set_mask(e4k, E4K_REG_DC3, 0x3f, qofs);
	if(res < 0)
		return res;

	res = e4k_reg_set_mask(e4k, E4K_REG_DC4, 0x33, (qrange << 4) | irange);
	return res;
}

/*! \brief Perform a DC offset calibration right now
 *  \param[e4k] handle to the tuner chip
 */
int e4k_dcOffsetCalibrate(E4KCtx* e4k)
{
	/* make sure the DC range detector is enabled */
	e4k_reg_set_mask(e4k, E4K_REG_DC5, E4K_DC5_RANGE_DET_EN, E4K_DC5_RANGE_DET_EN);

	return e4k_reg_write(e4k, E4K_REG_DC1, 0x01);
}


static const int8_t if_gains_max[] = {
	0, 6, 9, 9, 2, 15, 15
};

struct gain_comb {
	int8_t mixer_gain;
	int8_t if1_gain;
	uint8_t reg;
};

static const struct gain_comb dc_gain_comb[] = {
	{ 4,  -3, 0x50 },
	{ 4,   6, 0x51 },
	{ 12, -3, 0x52 },
	{ 12,  6, 0x53 },
};

#define TO_LUT(offset, range)	(offset | (range << 6))

int e4k_dcOffsetGenTable(E4KCtx* e4k)
{
	uint32_t i;

	/* FIXME: read ont current gain values and write them back
	 * before returning to the caller */

	/* disable auto mixer gain */
	e4k_reg_set_mask(e4k, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO, 0);

	/* set LNA/IF gain to full manual */
	e4k_reg_set_mask(e4k, E4K_REG_AGC1, E4K_AGC1_MOD_MASK,
			 E4K_AGC_MOD_SERIAL);

	/* set all 'other' gains to maximum */
	for (i = 2; i <= 6; i++)
		e4k_ifGainSet(e4k, i, if_gains_max[i]);

	/* iterate over all mixer + if_stage_1 gain combinations */
	for (i = 0; i < ARRAY_SIZE(dc_gain_comb); i++) {
		uint8_t offs_i, offs_q, range, range_i, range_q;

		/* set the combination of mixer / if1 gain */
		e4k_mixerGainSet(e4k, dc_gain_comb[i].mixer_gain);
		e4k_ifGainSet(e4k, 1, dc_gain_comb[i].if1_gain);

		/* perform actual calibration */
		e4k_dcOffsetCalibrate(e4k);

		/* extract I/Q offset and range values */
		offs_i = e4k_reg_read(e4k, E4K_REG_DC2) & 0x3f;
		offs_q = e4k_reg_read(e4k, E4K_REG_DC3) & 0x3f;
		range  = e4k_reg_read(e4k, E4K_REG_DC4);
		range_i = range & 0x3;
		range_q = (range >> 4) & 0x3;
/*
		fprintf(stderr, "Table %u I=%u/%u, Q=%u/%u\n",
			i, range_i, offs_i, range_q, offs_q);
*/
		/* write into the table */
		e4k_reg_write(e4k, dc_gain_comb[i].reg,
			      TO_LUT(offs_q, range_q));
		e4k_reg_write(e4k, dc_gain_comb[i].reg + 0x10,
			      TO_LUT(offs_i, range_i));
	}

	return 0;
}

/***********************************************************************
 * Initialization */

static int magic_init(E4KCtx* e4k)
{
	if(e4k_reg_write(e4k, 0x7e, 0x01) < 0)
		return -1;
	if(e4k_reg_write(e4k, 0x7f, 0xfe) < 0)
		return -1;
	if(e4k_reg_write(e4k, 0x82, 0x00) < 0)
		return -1;
	if(e4k_reg_write(e4k, 0x86, 0x50) < 0)	/* polarity A */
		return -1;
	if(e4k_reg_write(e4k, 0x87, 0x20) < 0)
		return -1;
	if(e4k_reg_write(e4k, 0x88, 0x01) < 0)
		return -1;
	if(e4k_reg_write(e4k, 0x9f, 0x7f) < 0)
		return -1;
	if(e4k_reg_write(e4k, 0xa0, 0x07) < 0)
		return -1;
	return 0;
}

/*! \brief Initialize the E4K tuner
 */
int e4k_reInit(E4KCtx* e4k)
{
	/* make a dummy i2c read or write command, will not be ACKed! */
	uint8_t dummy;
	e4k->twi->read(e4k->i2c_addr, 0, 1, &dummy, 1);

	/* Make sure we reset everything and clear POR indicator */
	e4k_reg_write(e4k, E4K_REG_MASTER1,
		E4K_MASTER1_RESET |
		E4K_MASTER1_NORM_STBY |
		E4K_MASTER1_POR_DET
	);

	/* Configure clock input */
	e4k_reg_write(e4k, E4K_REG_CLK_INP, 0x00);

	/* Disable clock output */
	e4k_reg_write(e4k, E4K_REG_REF_CLK, 0x00);
	e4k_reg_write(e4k, E4K_REG_CLKOUT_PWDN, 0x96);

	/* Write some magic values into registers */
	magic_init(e4k);

	/* Set common mode voltage a bit higher for more margin */
	e4k_commonModeSet(e4k, 7);
#if 0
	/* Initialize DC offset lookup tables */
	e4k_dc_offset_gen_table(e4k);

	/* Enable time variant DC correction */
	e4k_reg_write(e4k, E4K_REG_DCTIME1, 0x01);
	e4k_reg_write(e4k, E4K_REG_DCTIME2, 0x01);
#endif

	/* Set LNA mode to manual */
	e4k_reg_write(e4k, E4K_REG_AGC4, 0x10); /* High threshold */
	e4k_reg_write(e4k, E4K_REG_AGC5, 0x04);	/* Low threshold */
	e4k_reg_write(e4k, E4K_REG_AGC6, 0x1a);	/* LNA calib + loop rate */

	e4k_reg_set_mask(e4k, E4K_REG_AGC1, E4K_AGC1_MOD_MASK,
		E4K_AGC_MOD_SERIAL);

	/* Set Mixer Gain Control to manual */
	e4k_reg_set_mask(e4k, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO, 0);

#if 0
	/* Enable LNA Gain enhancement */
	e4k_reg_set_mask(e4k, E4K_REG_AGC11, 0x7,
			 E4K_AGC11_LNA_GAIN_ENH | (2 << 1));

	/* Enable automatic IF gain mode switching */
	e4k_reg_set_mask(e4k, E4K_REG_AGC8, 0x1, E4K_AGC8_SENS_LIN_AUTO);
#endif

	/* Use auto-gain as default */
	e4k_enableManualGain(e4k, 0);

	/* Select moderate gain levels */
	e4k_ifGainSet(e4k, 1, 6);
	e4k_ifGainSet(e4k, 2, 0);
	e4k_ifGainSet(e4k, 3, 0);
	e4k_ifGainSet(e4k, 4, 0);
	e4k_ifGainSet(e4k, 5, 9);
	e4k_ifGainSet(e4k, 6, 9);

	/* Set the most narrow filter we can possibly use */
	e4k_ifFilterBWSet(e4k, E4K_IF_FILTER_MIX, KHZ(1900));
	e4k_ifFilterBWSet(e4k, E4K_IF_FILTER_RC, KHZ(1000));
	e4k_ifFilterBWSet(e4k, E4K_IF_FILTER_CHAN, KHZ(2150));
	e4k_ifFilterChanEnable(e4k, True);

	/* Disable time variant DC correction and LUT */
	e4k_reg_set_mask(e4k, E4K_REG_DC5, 0x03, 0);
	e4k_reg_set_mask(e4k, E4K_REG_DCTIME1, 0x03, 0);
	e4k_reg_set_mask(e4k, E4K_REG_DCTIME2, 0x03, 0);

	return 0;
}

/** external access **/

void e4k_configure(E4KCtx* ctx, const TWI* twi, u8 addr, u32 refFreq, const PinConfig* powerPins, uint numPowerPins)
{
	ctx->twi = twi;
	ctx->i2c_addr = addr;
	ctx->vco.fosc = refFreq * 1000;
	ctx->powerPins = powerPins;
	ctx->numPowerPins = numPowerPins;

	pio_configure(ctx->powerPins, ctx->numPowerPins);
	e4k_reInit(ctx);
}

void e4k_setPower(E4KCtx* ctx, Bool up)
{
	if(up) {
		for(int i = 0; i < ctx->numPowerPins; i++)
			pio_set(&ctx->powerPins[i]);
	} else {
		for(int i = 0; i < ctx->numPowerPins; i++)
			pio_clear(&ctx->powerPins[i]);
	}
}

void e4k_dump(E4KCtx* e4k)
{
	printf("E4K register dump:\n");
	for(int i = 0; i < 64; i++)
		printf("0x%02x: 0x%02x    0x%02x: 0x%02x    0x%02x: 0x%02x    0x%02x: 0x%02x\n\r",
			i, e4k_reg_read(e4k, i),
			i + 64, e4k_reg_read(e4k, i + 64),
			i + 128, e4k_reg_read(e4k, i + 128),
			i + 192, e4k_reg_read(e4k, i + 192)
	);
}

int e4k_setReg(E4KCtx* e4k, u8 reg, u8 val)
{
	return e4k_reg_write(e4k, reg, val);
}


/*! \brief High-level tuning API, just specify frquency
 *
 *  This function will compute matching PLL parameters, program them into the
 *  hardware and set the band as well as RF filter.
 *
 *  \param[in] e4k reference to tuner
 *  \param[in] freq frequency in Hz
 *  \returns actual tuned frequency, negative in case of error
 */
int e4k_tune(E4KCtx* e4k, u32 freq)
{
	int rc;
	struct e4k_pll_params p;
	int tries;

	e4k->frequency = freq;

	/* determine PLL parameters */
	rc = e4k_compute_pll_params(&p, e4k->vco.fosc, freq * 1000);
	if(!rc)
		return -EINVAL;

	/* actually tune to those parameters */
	e4k_tune_params(e4k, &p);
#define TRIES 100
	/* check PLL lock */
	for(rc = 0, tries = TRIES; (tries > 0) && (!(rc & 0x01)); tries--) {
		rc = e4k_reg_read(e4k, E4K_REG_SYNTH1);
		if(rc < 0)
			return -1;
	}
	if(tries == 0) {
		printf("E4K: PLL does not lock at %u kHz!\n", freq);
		e4k->lock = False;
		return -1;
	} else {
		e4k->lock = True;
		return 0;
	}
}
