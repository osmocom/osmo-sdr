#ifndef INCLUDE_SAM3U_H
#define INCLUDE_SAM3U_H

#include <stdint.h>

int sam3uRead32(int fd, uint32_t address, uint32_t* value);
int sam3uRead16(int fd, uint32_t address, uint16_t* value);
int sam3uRead8(int fd, uint32_t address, uint8_t* value);

int sam3uWrite32(int fd, uint32_t address, uint32_t value);
int sam3uWrite16(int fd, uint32_t address, uint16_t value);
int sam3uWrite8(int fd, uint32_t address, uint8_t value);

int sam3uRun(int fd, uint32_t address);
int sam3uDetect(int fd, uint32_t* chipID);
int sam3uReadUniqueID(int fd, int bank, uint8_t* uniqueID);
int sam3uFlash(int fd, int bank, void* bin, size_t binSize);

#endif // INCLUDE_SAM3U_H
