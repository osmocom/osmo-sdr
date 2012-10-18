#ifndef INCLUDE_FLASH_H
#define INCLUDE_FLASH_H

#include "../crt/types.h"

void flash_readUID(char* uid);
int flash_write_bank0(u32 addr, const u8* data, uint len);
void flash_write_loader(const u8* data);

#endif // INCLUDE_FLASH_H
