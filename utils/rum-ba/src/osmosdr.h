#ifndef INCLUDE_OSMOSDR_H
#define INCLUDE_OSMOSDR_H

#include <stdint.h>

int osmoSDRDetect(int fd);
int osmoSDRPrintUID(int fd, int bank);
int osmoSDRBlink(int fd);
int osmoSDRRamLoad(int fd, void* bin, size_t binSize);
int osmoSDRFlash(int fd, void* bin, size_t binSize);

#endif // INCLUDE_OSMOSDR_H
