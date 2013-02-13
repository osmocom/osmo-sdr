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

#include "flash.h"
#include "../crt/stdio.h"
#include "../at91sam3u4/AT91SAM3U4.h"
#include "../board.h"
#include "../at91sam3u4/core_cm3.h"
#include "sys.h"

static __attribute__((section(".ramfunc"))) uint32_t _flash_get_FAULTMASK(void)
{
  uint32_t result=0;

  __ASM volatile ("MRS %0, faultmask" : "=r" (result) );
  return(result);
}

static __attribute__((section(".ramfunc"))) void _flash_set_FAULTMASK(uint32_t faultMask)
{
  __ASM volatile ("MSR faultmask, %0" : : "r" (faultMask) );
}

__attribute__((section(".ramfunc"))) void flash_readUID(char* uid)
{
	u32 faultmask = _flash_get_FAULTMASK() ;
	__disable_fault_irq();

	__ISB();
	__DSB();

	while((AT91C_BASE_EFC0->EFC_FSR & AT91C_EFC_FRDY_S) != AT91C_EFC_FRDY_S) __NOP();

	AT91C_BASE_EFC0->EFC_FCR = 0x5a00000e;
	while((AT91C_BASE_EFC0->EFC_FSR & AT91C_EFC_FRDY_S) == AT91C_EFC_FRDY_S) __NOP();

	for(int i = 0; i < 16; i++)
		uid[i] = ((volatile u8*)AT91C_IFLASH0)[i];
	uid[16] = '\0';

	AT91C_BASE_EFC0->EFC_FCR = 0x5a00000f;
	while((AT91C_BASE_EFC0->EFC_FSR & AT91C_EFC_FRDY_S) != AT91C_EFC_FRDY_S) __NOP();

	// reset cacheline
	((volatile u32*)AT91C_IFLASH0)[0];
	__ISB();
	__DSB();

	_flash_set_FAULTMASK(faultmask);
}

__attribute__((section(".ramfunc"))) int flash_write_bank0(u32 addr, const u8* data, uint len)
{
	u32 faultmask = _flash_get_FAULTMASK();
	AT91PS_EFC efc = AT91C_BASE_EFC0;
	u32* flashBase = (u32*)AT91C_IFLASH0;
	u32 blockLen;
	int res = 0;

	// make sure we're starting on a new page
	if((addr & (AT91C_IFLASH0_PAGE_SIZE - 1)) != 0)
		return -1;

	__disable_fault_irq();

	__ISB();
	__DSB();

	AT91C_BASE_EFC0->EFC_FMR = ((6 << 8) & AT91C_EFC_FWS);
	AT91C_BASE_EFC1->EFC_FMR = ((6 << 8) & AT91C_EFC_FWS);

	while((efc->EFC_FSR & AT91C_EFC_FRDY_S) != AT91C_EFC_FRDY_S) __NOP();

	while(len > 0) {
		blockLen = len;
		if(blockLen > AT91C_IFLASH0_PAGE_SIZE)
			blockLen = AT91C_IFLASH0_PAGE_SIZE;

		// send data to flash
		for(int i = 0; i < blockLen / 4; i++)
			flashBase[i] = *(const u32*)&data[i * 4];

		// erase + program
		efc->EFC_FCR = 0x5a000000 | (((addr - AT91C_IFLASH0) / AT91C_IFLASH0_PAGE_SIZE) << 8) | 0x03;

		// wait until finished
		while((efc->EFC_FSR & AT91C_EFC_FRDY_S) == AT91C_EFC_FRDY_S) __NOP();
		while((efc->EFC_FSR & AT91C_EFC_FRDY_S) != AT91C_EFC_FRDY_S) __NOP();

		// verify
		for(int i = 0; i < blockLen; i++) {
			if((*(const u8*)(addr + i)) != data[i]) {
				res = -1;
				break;
			}
		}

		addr += blockLen;
		data += blockLen;
		len -= blockLen;
	}

	AT91C_BASE_EFC0->EFC_FMR = AT91C_EFC_FWS_2WS;
	AT91C_BASE_EFC1->EFC_FMR = AT91C_EFC_FWS_2WS;

	// reset cacheline
	((volatile u32*)AT91C_IFLASH0)[0];
	__ISB();
	__DSB();

	_flash_set_FAULTMASK(faultmask);

	return res;
}
