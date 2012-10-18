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

#include "sys.h"
#include "../at91sam3u4/AT91SAM3U4.h"
#include "../board.h"
#include "../at91sam3u4/core_cm3.h"

void sys_reset(void)
{
	__disable_fault_irq();
	__disable_irq();
	AT91C_BASE_RSTC->RSTC_RCR = 0xa5000000 | AT91C_RSTC_PROCRST | AT91C_RSTC_ICERST | AT91C_RSTC_PERRST | AT91C_RSTC_EXTRST;
	while(1) ;
}
