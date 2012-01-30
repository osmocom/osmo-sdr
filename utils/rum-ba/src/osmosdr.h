#ifndef INCLUDE_OSMOSDR_H
#define INCLUDE_OSMOSDR_H

#include <stdint.h>
#include "utils.h"

int osmoSDRDetect(HANDLE fd);
int osmoSDRPrintUID(HANDLE fd, int bank);
int osmoSDRBlink(HANDLE fd);
int osmoSDRRamLoad(HANDLE fd, const void* bin, size_t binSize);
int osmoSDRFlashMCU(HANDLE fd, const void* bin, size_t binSize);
int osmoSDRFlashFPGA(HANDLE fd, const void* algo, size_t algoSize, const void* bin, size_t binSize);

#endif // INCLUDE_OSMOSDR_H
