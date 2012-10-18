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

#include "../crt/stdio.h"
#include "twi.h"
#include "si570.h"

static const SI570Info g_si570Info[2] = {
	{
		.freqRange = {
			{
				.min = 10000,
				.max = 945000
			},
			{
				.min = 970000,
				.max = 1134000},
			{
				.min = 1213000,
				.max = 1417500}
		},
		.initFreq = 856000
	},
	{
		.freqRange = {
			{
				.min = 10000,
				.max = 280000
			}
		},
		.initFreq = 100000
	}
};
static int hsToDiv(int n1)

{
	switch(n1) {
		case 0:
			return 4;
		case 1:
			return 5;
		case 2:
			return 6;
		case 3:
			return 7;
		case 5:
			return 9;
		case 7:
			return 11;
	}

	return 0;
}

static int smbus8_read_bytes(const TWI* twi, u8 addr, u8 reg, u8* out, uint num)
{
	if(twi->read(addr, reg, 1, out, num) < 0) {
		printf("SI570: I2C address 0x%02x read error.\n", addr);
		return -1;
	} else {
		return 0;
	}
}

static int smbus8_write_bytes(const TWI* twi, u8 addr, u8 reg, const u8* val, uint num)
{
	if(twi->write(addr, reg, 1, val, num) < 0) {
		printf("SI570: I2C address 0x%02x write error.\n", addr);
		return -1;
	} else {
		return 0;
	}
}

static int smbus8_write_byte(const TWI* twi, u8 addr, u8 reg, u8 val)
{
	return smbus8_write_bytes(twi, addr, reg, &val, 1);
}

static int reset(SI570Ctx* ctx)
{
	if(smbus8_write_byte(ctx->twi, ctx->addr, 135, 80) < 0)
		return -1;
	for(volatile int i = 0; i < 1000000; i++) ;
	if(smbus8_write_byte(ctx->twi, ctx->addr, 135, 01) < 0)
		return -1;
	for(volatile int i = 0; i < 1000000; i++) ;

	return 0;
}

int readCalibration(SI570Ctx* ctx)
{
	int n;
	u8 data[6];
	u64 xd, xf;
	int n1, hsDiv;

	for(int i = 0; i < 6; i++)
		data[i] = 0;

	if(smbus8_read_bytes(ctx->twi, ctx->addr, 7, data, 6) < 0) {
		printf("SI570: Error reading calibration data.\n");
		return -1;
	}
/*
	printf("SI570: Calibration data:");
	for(int i = 0; i < 6; i++)
		printf(" 0x%02x", data[i]);
	printf("\n");
*/
	xd = data[1] & 0x3f;

	xd <<= 8;
	xd |= data[2];

	xd <<= 8;
	xd |= data[3];

	xd <<= 8;
	xd |= data[4];

	xd <<= 8;
	xd |= data[5];

	n1 = ((data[0] << 2) & 0x1F) | (data[1] >> 6);
	hsDiv = (data[0] >> 5);

	n = 1;
	if(xd > 0) {
		xf = g_si570Info[n].initFreq * 1000;
		xf *= hsToDiv(hsDiv) * (n1 + 1);
		xf <<= 31;

		ctx->xtal = (xf + (xd / 2)) / xd;
		ctx->info = &g_si570Info[n];

		return 0;
	} else {
		printf("SI570: Calibration data invalid.\n");
		return -1;
	}
}

void si570_configure(SI570Ctx* ctx, const TWI* twi, u8 addr)
{
	ctx->twi = twi;
	ctx->addr = addr;
	ctx->ready = False;
	ctx->lock = False;
	si570_init(ctx);
}

int si570_init(SI570Ctx* ctx)
{
	if(reset(ctx) < 0) {
		printf("SI570: Reset failed.\n");
		return -1;
	}

	if(readCalibration(ctx) < 0) {
		printf("SI570: Calibration data access failed.\n");
		return -1;
	}

	ctx->ready = True;
	return 0;
}

int si570_setFrequency(SI570Ctx* ctx, u32 freq)
{
	u32 dco_min = 4850000ULL;
	int mul, hsv, n1;

	if(!ctx->ready) {
		printf("SI570: Cannot set frequency when not properly initialized.\n");
		return -1;
	}

	ctx->lock = False;

	for(mul = (dco_min / freq) + 1; (freq * mul) <= 5670000; mul++) {
		for(hsv = 7; hsv >= 0; hsv--) {
			int hsdiv = hsToDiv(hsv);

			if(!hsdiv)
				continue;
			if(mul % hsdiv)
				continue;

			n1 = mul / hsdiv;

			if(n1 > 128)
				continue;

			if(n1 != 1) {
				if(n1 < 1)
					continue;
				if(n1 % 2)
					continue;
			}

			goto found_solution;
		}
	}

	printf("SI570: Could not set frequency to %u kHz.\n", freq);
	return -1;

found_solution:
	{
		u8 data[6];
		u64 xf, xd, xr;
		u32 n1v;

		xf = freq;
		xf *= 1000;
		xf += ctx->trim;

		xd = hsToDiv(hsv) * n1 * xf;
		xd <<= 31;

		xr = (xd + ctx->xtal / 2) / ctx->xtal;

		n1v = n1 - 1;

		data[0] = (hsv << 5) | (n1v >> 2);
		data[1] = (n1v << 6) | (xr >> 32);
		data[2] = (xr >> 24);
		data[3] = (xr >> 16);
		data[4] = (xr >> 8);
		data[5] = (xr >> 0);

		if(smbus8_write_byte(ctx->twi, ctx->addr, 137, 0x10) < 0)
			return -1;
		if(smbus8_write_bytes(ctx->twi, ctx->addr, 7, data, 6) < 0)
			return -1;
		if(smbus8_write_byte(ctx->twi, ctx->addr, 137, 0x00) < 0)
			return -1;
		if(smbus8_write_byte(ctx->twi, ctx->addr, 135, 0x40) < 0)
			return -1;

		ctx->lock = True;
		if(ctx->frequency != freq) {
			ctx->frequency = freq;
			ctx->ppsValid = 0;
		}
	}
	return 0;
}

int si570_setFrequencyAndTrim(SI570Ctx* ctx, u32 freq, u32 trim)
{
	ctx->trim = trim;
	return si570_setFrequency(ctx, freq);
}

u32 si570_getFrequency(SI570Ctx* ctx)
{
	return ctx->frequency;
}

Bool si570_isLocked(SI570Ctx* ctx)
{
	if(!ctx->ready)
		return False;
	if(!ctx->lock)
		return False;
	return True;
}

void si570_ppsUpdate(SI570Ctx* ctx, u32 cnt, Bool reset)
{
	u32 prec = ctx->frequency / 100;
	u32 cntKhz = cnt / 1000;
	if((cntKhz < (ctx->frequency - prec)) || (cntKhz > (ctx->frequency + prec))) {
		printf("SI570 PPS: Current counter value not valid (%u Hz).\n", cnt);
		return;
	}

	if(reset)
		ctx->ppsValid = 0;

	if(ctx->ppsValid == 0)
		ctx->ppsCnt = cnt;
	else ctx->ppsCnt = (ctx->ppsCnt * 9 + cnt * 1) / 10;
	ctx->ppsValid++;

	if(ctx->ppsValid >= 10) {
		ctx->ppsValid = 0;
		ctx->trim += ((s64)ctx->frequency * 1000) - ctx->ppsCnt;
		si570_setFrequency(ctx, ctx->frequency);
	}
	printf("SI570 PPS: Current count: %u Hz, trim: %d Hz (%d pulses until update).\n", ctx->ppsCnt, ctx->trim, 10 - ctx->ppsValid);
}

int si570_regWrite(SI570Ctx* ctx, u8 reg, uint len, const u8* data)
{
	return smbus8_write_bytes(ctx->twi, ctx->addr, reg, data, len);
}
