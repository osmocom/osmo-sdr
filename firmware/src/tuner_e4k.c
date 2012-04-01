
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

#include <common.h>
#include <logging.h>
#include <reg_field.h>

#include <tuner_e4k.h>

/* If this is defined, the limits are somewhat relaxed compared to what the
 * vendor claims is possible */
#define OUT_OF_SPEC

#define MHZ(x)	((x)*1000*1000)
#define KHZ(x)	((x)*1000)

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

#if 0
/*! \brief Write a register of the tuner chip
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \param[in] val value to be written
 *  \returns 0 on success, negative in case of error
 */
int e4k_reg_write(struct e4k_state *e4k, uint8_t reg, uint8_t val)
{
	/* FIXME */
	return 0;
}

/*! \brief Read a register of the tuner chip
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \returns positive 8bit register contents on success, negative in case of error
 */
int e4k_reg_read(struct e4k_state *e4k, uint8_t reg)
{
	/* FIXME */
	return 0;
}
#endif

/*! \brief Set or clear some (masked) bits inside a register
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \param[in] mask bit-mask of the value
 *  \param[in] val data value to be written to register
 *  \returns 0 on success, negative in case of error
 */
static int e4k_reg_set_mask(struct e4k_state *e4k, uint8_t reg,
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
static int e4k_field_write(struct e4k_state *e4k, const struct reg_field *field, uint8_t val)
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
static int e4k_field_read(struct e4k_state *e4k, const struct reg_field *field)
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
	unsigned int i, bi;
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
		if (freq < MHZ(268))
			rc = 0;
		else
			rc = 8;
		break;
	case E4K_BAND_VHF3:
		if (freq < MHZ(509))
			rc = 0;
		else
			rc = 8;
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
		rc -EINVAL;
		break;
	}

	return rc;
}

/* \brief Automatically select apropriate RF filter based on e4k state */
int e4k_rf_filter_set(struct e4k_state *e4k)
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
	[E4K_IF_FILTER_MIX] = mix_filter_bw,
	[E4K_IF_FILTER_CHAN] = ifch_filter_bw,
	[E4K_IF_FILTER_RC] = ifrc_filter_bw,
};

static const uint32_t if_filter_bw_len[] = {
	[E4K_IF_FILTER_MIX] = ARRAY_SIZE(mix_filter_bw),
	[E4K_IF_FILTER_CHAN] = ARRAY_SIZE(ifch_filter_bw),
	[E4K_IF_FILTER_RC] = ARRAY_SIZE(ifrc_filter_bw),
};

static const struct reg_field if_filter_fields[] = {
	[E4K_IF_FILTER_MIX] = {
		.reg = E4K_REG_FILT2, .shift = 4, .width = 4,
	},
	[E4K_IF_FILTER_CHAN] = {
		.reg = E4K_REG_FILT3, .shift = 0, .width = 5,
	},
	[E4K_IF_FILTER_RC] = {
		.reg = E4K_REG_FILT2, .shift = 0, .width = 4,
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
int e4k_if_filter_bw_set(struct e4k_state *e4k, enum e4k_if_filter filter,
		         uint32_t bandwidth)
{
	int bw_idx;
	uint8_t mask;
	const struct reg_field *field;

	if (filter >= ARRAY_SIZE(if_filter_bw))
		return -EINVAL;

	bw_idx = find_if_bw(filter, bandwidth);

	field = &if_filter_fields[filter];

	return e4k_field_write(e4k, field, bw_idx);
}

int e4k_if_filter_bw_get(struct e4k_state *e4k, enum e4k_if_filter filter)
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
#define E4K_FLO_MAX_MHZ		1900
#else
#define E4K_FLO_MIN_MHZ		64
#define E4K_FLO_MAX_MHZ		1700
#endif

/* \brief table of R dividers in case 3phase mixing is enabled,
 * the values have to be halved if it's 2phase */
static const uint8_t vco_r_table_3ph[] = {
	4, 8, 12, 16, 24, 32, 40, 48
};

static int is_fvco_valid(uint32_t fvco_z)
{
	/* check if the resulting fosc is valid */
	if (fvco_z/1000 < E4K_FVCO_MIN_KHZ ||
	    fvco_z/1000 > E4K_FVCO_MAX_KHZ) {
		LOGP(DTUN, LOGL_ERROR, "Fvco %u invalid\n", fvco_z);
		return 0;
	}

	return 1;
}

static int is_fosc_valid(uint32_t fosc)
{
	if (fosc < MHZ(16) || fosc > MHZ(30)) {
		LOGP(DTUN, LOGL_ERROR, "Fosc %u invalid\n", fosc);
		return 0;
	}

	return 1;
}

static int is_flo_valid(uint32_t flo)
{
	if (flo < MHZ(E4K_FLO_MIN_MHZ) || flo > MHZ(E4K_FLO_MAX_MHZ)) {
		LOGP(DTUN, LOGL_ERROR, "Flo %u invalid\n", flo);
		return 0;
	}

	return 1;
}

static int is_z_valid(uint32_t z)
{
	if (z > 255) {
		LOGP(DTUN, LOGL_ERROR, "Z %u invalid\n", z);
		return 0;
	}

	return 1;
}

/*! \brief Determine if 3-phase mixing shall be used or not */
static int use_3ph_mixing(uint32_t flo)
{
	/* this is a magic number somewhre between VHF and UHF */
	if (flo < MHZ(300))
		return 1;

	return 0;
}

/* \brief compute Fvco based on Fosc, Z and X
 * \returns positive value (Fvco in Hz), 0 in case of error */
static unsigned int compute_fvco(uint32_t f_osc, uint8_t z, uint16_t x)
{
	uint64_t fvco_z, fvco_x, fvco;

	/* We use the following transformation in order to
	 * handle the fractional part with integer arithmetic:
	 *  Fvco = Fosc * (Z + X/Y) <=> Fvco = Fosc * Z + (Fosc * X)/Y
	 * This avoids X/Y = 0.  However, then we would overflow a 32bit
	 * integer, as we cannot hold e.g. 26 MHz * 65536 either.
	 */
	fvco_z = (uint64_t)f_osc * z;

	if (!is_fvco_valid(fvco_z))
		return 0;

	fvco_x = ((uint64_t)f_osc * x) / E4K_PLL_Y;

	fvco = fvco_z + fvco_x;

	/* this shouldn't happen, but better to check explicitly for integer
	 * overflows before converting uint64_t to "int" */
	if (fvco > UINT_MAX) {
		LOGP(DTUN, LOGL_ERROR, "Fvco %llu > INT_MAX\n", fvco);
		return 0;
	}

	return fvco;
}

static int compute_flo(uint32_t f_osc, uint8_t z, uint16_t x, uint8_t r)
{
	unsigned int fvco = compute_fvco(f_osc, z, x);
	if (fvco == 0)
		return -EINVAL;

	return fvco / r;
}

static int e4k_band_set(struct e4k_state *e4k, enum e4k_band band)
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

	rc = e4k_reg_set_mask(e4k, E4K_REG_SYNTH1, 0x06, band << 1);
	if (rc >= 0)
		e4k->band = band;

	return rc;
}

#if 0
static int compute_lowest_r_idx(uint32_t flo, uint32_t fosc)
{
	int three_phase_mixing = use_3ph_mixing(intended_flo);
	uint32_t r_ideal;

	/* determine what would be the idael R divider, taking into account
	 * fractional remainder of the division */
	r_ideal = flo / fosc;
	if (flo % fosc)
		r_ideal++;

	/* find the next best (bigger) possible R value */
	for (i = 0; i < ARRAY_SIZE(vco_r_table_3ph); i++) {
		uint32_t r = vco_r_table_3ph[i];

		if (!three_phase_mixing)
			r = r / 2;

		if (r < r_ideal)
			continue;

		return i;
	}

	/* this shouldn't happen!!! */
	return 0;
}
#endif

/*! \brief Compute PLL parameters for givent target frequency
 *  \param[out] oscp Oscillator parameters, if computation successful
 *  \param[in] fosc Clock input frequency applied to the chip (Hz)
 *  \param[in] intended_flo target tuning frequency (Hz)
 *  \returns actual PLL frequency, as close as possible to intended_flo,
 *	     negative in case of error
 */
int e4k_compute_pll_params(struct e4k_pll_params *oscp, uint32_t fosc, uint32_t intended_flo)
{
	int i;
	int three_phase_mixing = use_3ph_mixing(intended_flo);

	if (!is_fosc_valid(fosc))
		return -EINVAL;

	if (!is_flo_valid(intended_flo))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(vco_r_table_3ph); i++) {
		uint8_t r = vco_r_table_3ph[i];
		uint64_t intended_fvco, z, remainder;
		uint32_t x;
		int flo;

		if (!three_phase_mixing)
			r = r / 2;

		LOGP(DTUN, LOGL_DEBUG, "Fint=%u, R=%u\n", intended_flo, r);

		/* flo(max) = 1700MHz, R(max) = 48, we need 64bit! */
		intended_fvco = (uint64_t)intended_flo * r;
		/* check if fvco is in range, if not continue */
		if (intended_fvco > UINT_MAX) {
			LOGP(DTUN, LOGL_DEBUG, "intended_fvco > UINT_MAX\n");
			continue;
		}

		if (!is_fvco_valid(intended_fvco))
			continue;

		/* compute integral component of multiplier */
		z = intended_fvco / fosc;
		if (!is_z_valid(z))
			continue;

		/* compute fractional part.  this will not overflow,
		 * as fosc(max) = 30MHz and z(max) = 255 */
		remainder = intended_fvco - (fosc * z);
		/* remainder(max) = 30MHz, E4K_PLL_Y = 65536 -> 64bit! */
		x = (remainder * E4K_PLL_Y) / fosc;
		/* x(max) as result of this computation is 65536 */

		flo = compute_flo(fosc, z, x, r);
		if (flo < 0)
			continue;

		oscp->fosc = fosc;
		oscp->flo = flo;
		oscp->intended_flo = intended_flo;
		oscp->r = r;
		oscp->r_idx = i;
		oscp->threephase = three_phase_mixing;
		oscp->x = x;
		oscp->z = z;

		return flo;
	}

	LOGP(DTUN, LOGL_ERROR, "No valid set of PLL params found for %u\n",
		intended_flo);
	return -EINVAL;
}

int e4k_tune_params(struct e4k_state *e4k, struct e4k_pll_params *p)
{
	uint8_t val;

	/* program R index + 3phase/2phase */
	val = (p->r_idx & 0x7) | ((p->threephase & 0x1) << 3);
	e4k_reg_write(e4k, E4K_REG_SYNTH7, val);
	/* program Z */
	e4k_reg_write(e4k, E4K_REG_SYNTH3, p->z);
	/* program X */
	e4k_reg_write(e4k, E4K_REG_SYNTH4, p->x & 0xff);
	e4k_reg_write(e4k, E4K_REG_SYNTH5, p->x >> 8);

	/* we're in auto calibration mode, so there's no need to trigger it */

	memcpy(&e4k->vco, p, sizeof(e4k->vco));

	/* set the band */
	if (e4k->vco.flo < MHZ(139))
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

/*! \brief High-level tuning API, just specify frquency
 *
 *  This function will compute matching PLL parameters, program them into the
 *  hardware and set the band as well as RF filter.
 *
 *  \param[in] e4k reference to tuner
 *  \param[in] freq frequency in Hz
 *  \returns actual tuned frequency, negative in case of error
 */
int e4k_tune_freq(struct e4k_state *e4k, uint32_t freq)
{
	int rc;
	struct e4k_pll_params p;

	/* determine PLL parameters */
	rc = e4k_compute_pll_params(&p, e4k->vco.fosc, freq);
	if (rc < 0)
		return rc;

	/* actually tune to those parameters */
	return e4k_tune_params(e4k, &p);
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
	[1] = if_stage1_gain,
	[2] = if_stage23_gain,
	[3] = if_stage23_gain,
	[4] = if_stage4_gain,
	[5] = if_stage56_gain,
	[6] = if_stage56_gain
};

static const uint8_t if_stage_gain_len[] = {
	[0] = 0,
	[1] = ARRAY_SIZE(if_stage1_gain),
	[2] = ARRAY_SIZE(if_stage23_gain),
	[3] = ARRAY_SIZE(if_stage23_gain),
	[4] = ARRAY_SIZE(if_stage4_gain),
	[5] = ARRAY_SIZE(if_stage56_gain),
	[6] = ARRAY_SIZE(if_stage56_gain)
};

static const struct reg_field if_stage_gain_regs[] = {
	[1] = { .reg = E4K_REG_GAIN3, .shift = 0, .width = 1 },
	[2] = { .reg = E4K_REG_GAIN3, .shift = 1, .width = 2 },
	[3] = { .reg = E4K_REG_GAIN3, .shift = 3, .width = 2 },
	[4] = { .reg = E4K_REG_GAIN3, .shift = 5, .width = 2 },
	[5] = { .reg = E4K_REG_GAIN4, .shift = 0, .width = 3 },
	[6] = { .reg = E4K_REG_GAIN4, .shift = 3, .width = 3 }
};

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
int e4k_if_gain_set(struct e4k_state *e4k, uint8_t stage, int8_t value)
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

int e4k_mixer_gain_set(struct e4k_state *e4k, int8_t value)
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

int e4k_commonmode_set(struct e4k_state *e4k, int8_t value)
{
	if(value < 0)
		return -EINVAL;
	else if(value > 7)
		return -EINVAL;

	return e4k_reg_set_mask(e4k, E4K_REG_DC7, 7, value);
}

/*********************************************************************** 
 * DC Offset */

int e4k_manual_dc_offset(struct e4k_state *e4k, int8_t iofs, int8_t irange, int8_t qofs, int8_t qrange)
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
int e4k_dc_offset_calibrate(struct e4k_state *e4k)
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
	uint8_t if1_gain;
	uint8_t reg;
};

static const struct gain_comb dc_gain_comb[] = {
	{ 4, -3, 0x50 },
	{ 4, 6, 0x51 },
	{ 12, -3, 0x52 },
	{ 12, 6, 0x53 },
};

#define TO_LUT(offset, range)	(offset | (range << 6))

int e4k_dc_offset_gen_table(struct e4k_state *e4k)
{
	int i;

	/* FIXME: read ont current gain values and write them back
	 * before returning to the caller */

	/* set all 'other' gains to maximum */
	for (i = 2; i <= 6; i++)
		e4k_if_gain_set(e4k, i, if_gains_max[i]);

	/* iterate over all mixer + if_stage_1 gain combinations */
	for (i = 0; i < ARRAY_SIZE(dc_gain_comb); i++) {
		uint8_t offs_i, offs_q, range_i, range_q;

		/* set the combination of mixer / if1 gain */
		e4k_mixer_gain_set(e4k, dc_gain_comb[i].mixer_gain);
		e4k_if_gain_set(e4k, 1, dc_gain_comb[i].if1_gain);

		/* perform actual calibration */
		e4k_dc_offset_calibrate(e4k);
		/* FIXME: do we have to wait? */

		/* extract I/Q offset and range values */
		offs_i = e4k_reg_read(e4k, E4K_REG_DC2) & 0x3F;
		offs_q = e4k_reg_read(e4k, E4K_REG_DC3) & 0x3F;
		range_i = e4k_reg_read(e4k, E4K_REG_DC4) & 0x3;
		range_q = (e4k_reg_read(e4k, E4K_REG_DC4) >> 4) & 0x3;

		LOGP(DTUN, LOGL_DEBUG, "Table %u I=%u/%u, Q=%u/%u\n",
			i, range_i, offs_i, range_q, offs_q);
		/* write into the table */
		e4k_reg_write(e4k, dc_gain_comb[i].reg,
			      TO_LUT(offs_q, range_q));
		e4k_reg_write(e4k, dc_gain_comb[i].reg+0x10,
			      TO_LUT(offs_i, range_i));
	}

	return 0;
}


/*********************************************************************** 
 * Initialization */

static int magic_init(struct e4k_state *e4k)
{
	e4k_reg_write(e4k, 0x7e, 0x01);
	e4k_reg_write(e4k, 0x7f, 0xfe);
	e4k_reg_write(e4k, 0x86, 0x50);	/* polarity A */
	e4k_reg_write(e4k, 0x87, 0x20);
	e4k_reg_write(e4k, 0x88, 0x01);
	e4k_reg_write(e4k, 0x9f, 0x7f);
	e4k_reg_write(e4k, 0xa0, 0x07);

	return 0;
}

/*! \brief Initialize the E4K tuner
 */
int e4k_init(struct e4k_state *e4k)
{
	/* make a dummy i2c read or write command, will not be ACKed! */
	e4k_reg_read(e4k, 0);

	/* write some magic values into registers */
	magic_init(e4k);

	/* Set LNA mode to autnonmous */
	e4k_reg_set_mask(e4k, E4K_REG_AGC1, E4K_AGC1_MOD_MASK,
			 E4K_AGC_MOD_IF_SERIAL_LNA_AUTON);

	/* Set Miser Gain Control to autonomous */
	e4k_reg_set_mask(e4k, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO,
					    E4K_AGC7_MIX_GAIN_AUTO);

	/* Enable LNA Gain enhancement */
	e4k_reg_set_mask(e4k, E4K_REG_AGC11, 0x7,
			 E4K_AGC11_LNA_GAIN_ENH | (2 << 1));

	/* Enable automatic IF gain mode switching */
	e4k_reg_set_mask(e4k, E4K_REG_AGC8, 0x1, E4K_AGC8_SENS_LIN_AUTO);

	/* FIXME: do we need to program Output Common Mode voltage ? */

	/* FIXME: initialize DC offset lookup tables */

	/* Disable Clock output, write 0x96 into 0x7A */
	e4k_reg_write(e4k, E4K_REG_CLKOUT_PWDN, E4K_CLKOUT_DISABLE);

	/* Clear the reset-detect register */
	e4k_reg_set_mask(e4k, E4K_REG_MASTER1, E4K_MASTER1_POR_DET, E4K_MASTER1_POR_DET);

	/* Set the most narrow filter we can possibly use */
	e4k_if_filter_bw_set(e4k, E4K_IF_FILTER_MIX, KHZ(1900));
	e4k_if_filter_bw_set(e4k, E4K_IF_FILTER_RC, KHZ(1000));
	e4k_if_filter_bw_set(e4k, E4K_IF_FILTER_CHAN, KHZ(2150));
#if 0
	/* Select moderate gain levels */
	e4k_if_gain_set(e4k, 1, 6);
	e4k_if_gain_set(e4k, 2, 3);
	e4k_if_gain_set(e4k, 3, 3);
	e4k_if_gain_set(e4k, 4, 1);
	e4k_if_gain_set(e4k, 5, 9);
	e4k_if_gain_set(e4k, 6, 9);
#endif
}
