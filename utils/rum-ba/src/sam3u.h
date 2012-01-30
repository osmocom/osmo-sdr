#ifndef INCLUDE_SAM3U_H
#define INCLUDE_SAM3U_H

#include <stdint.h>
#include "utils.h"

int sam3uRead32(HANDLE fd, uint32_t address, uint32_t* value);
int sam3uRead16(HANDLE fd, uint32_t address, uint16_t* value);
int sam3uRead8(HANDLE fd, uint32_t address, uint8_t* value);

int sam3uWrite32(HANDLE fd, uint32_t address, uint32_t value);
int sam3uWrite16(HANDLE fd, uint32_t address, uint16_t value);
int sam3uWrite8(HANDLE fd, uint32_t address, uint8_t value);

int sam3uRun(HANDLE fd, uint32_t address);
int sam3uDetect(HANDLE fd, uint32_t* chipID);
int sam3uReadUniqueID(HANDLE fd, int bank, uint8_t* uniqueID);
int sam3uFlash(HANDLE fd, int bank, const void* bin, size_t binSize);

#endif // INCLUDE_SAM3U_H
