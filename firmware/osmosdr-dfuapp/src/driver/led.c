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

#include "led.h"
#include "../at91sam3u4/AT91SAM3U4.h"

void led_configure()
{
	AT91C_BASE_PIOB->PIO_IDR = AT91C_PIO_PB18;
	AT91C_BASE_PIOB->PIO_PPUDR = AT91C_PIO_PB18;
	AT91C_BASE_PIOB->PIO_MDDR = AT91C_PIO_PB18;
	AT91C_BASE_PIOB->PIO_OER = AT91C_PIO_PB18;
	AT91C_BASE_PIOB->PIO_PER = AT91C_PIO_PB18;

	AT91C_BASE_PIOB->PIO_IDR = AT91C_PIO_PB19;
	AT91C_BASE_PIOB->PIO_PPUDR = AT91C_PIO_PB19;
	AT91C_BASE_PIOB->PIO_MDDR = AT91C_PIO_PB19;
	AT91C_BASE_PIOB->PIO_OER = AT91C_PIO_PB19;
	AT91C_BASE_PIOB->PIO_PER = AT91C_PIO_PB19;

	AT91C_BASE_PIOB->PIO_SODR = AT91C_PIO_PB18;
	AT91C_BASE_PIOB->PIO_SODR = AT91C_PIO_PB19;
}
