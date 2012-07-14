#ifndef __SI570_H__
#define __SI570_H__

/* (C) 2005-2011 by maintech GmbH
 * (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
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

#define SI570_ADDR  0x55
#define SI570_XTAL  114285000L

struct range {
	uint32_t min;
	uint32_t max;
};

struct si570_info {
	struct range freq_range[3];
	uint32_t   init_freq;
};

//device-context
struct si570_ctx {
	int init;
	int lock;
	void *i2c;
	uint8_t	slave_addr;

	uint32_t xtal;
	const struct si570_info *info;
};

//API
int si570_init(struct si570_ctx *ctx, void *i2c_dev, uint8_t i2c_addr);
int si570_reinit(struct si570_ctx *ctx);
int si570_read_calibration(struct si570_ctx *ctx);
int si570_reset(struct si570_ctx *ctx);
//mode of operation
int si570_get_lock(struct si570_ctx *ctx);
//frequency
int si570_set_freq(struct si570_ctx *ctx, uint32_t freq, int trim);
//dump all registers
void si570_regdump(struct si570_ctx *ctx);
//write register(s)
int si570_reg_write(struct si570_ctx *ctx, uint8_t reg, int len, const uint8_t* data);

#endif //__SI570_H__
