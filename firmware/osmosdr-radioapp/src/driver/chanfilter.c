/* (C) 2011-2012 by Christian Daniel <cd@maintech.de>
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

#include "chanfilter.h"
#include "../crt/stdio.h"

typedef struct {
	const TWI* twi;
} ChanFilterCtx;

static ChanFilterCtx g_chanFilterCtx;

void chanfilter_configure(const TWI* twi)
{
	g_chanFilterCtx.twi = twi;

	if(chanfilter_setAll(0xff, 0xff, 0xff, 0xff) < 0)
		printf("ChanFilter: initialisation error\n");
}

int chanfilter_set(uint poti, u8 value)
{
	switch(poti) {
		case 0:
			return g_chanFilterCtx.twi->write(0x28, 0x11, 1, &value, 1);
		case 1:
			return g_chanFilterCtx.twi->write(0x28, 0x12, 1, &value, 1);
		case 2:
			return g_chanFilterCtx.twi->write(0x29, 0x11, 1, &value, 1);
		case 3:
			return g_chanFilterCtx.twi->write(0x29, 0x12, 1, &value, 1);
		default:
			return -1;
	}
}

int chanfilter_setAll(u8 p0v0, u8 p0v1, u8 p1v0, u8 p1v1)
{
	if(chanfilter_set(0, p0v0) < 0)
		return -1;
	if(chanfilter_set(1, p0v1) < 0)
		return -1;
	if(chanfilter_set(2, p1v0) < 0)
		return -1;
	if(chanfilter_set(3, p1v1) < 0)
		return -1;
	return 0;
}

int chanfilter_regWrite(uint chip, u8 reg, u8 value)
{
	switch(chip) {
		case 0:
			return g_chanFilterCtx.twi->write(0x28, reg, 1, &value, 1);
		case 1:
			return g_chanFilterCtx.twi->write(0x29, reg, 1, &value, 1);
		default:
			return -1;
	}
}
