#ifndef INCLUDE_FLASH_H
#define INCLUDE_FLASH_H

#include "../crt/types.h"

void flash_readUID(char* uid);
int flash_write_bank0(u32 addr, const u8* data, uint len);

#endif // INCLUDE_FLASH_H
