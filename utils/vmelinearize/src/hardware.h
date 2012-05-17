#ifndef INCLUDE_HARDWARE_H
#define INCLUDE_HARDWARE_H

#include <stdint.h>

const uint8_t* g_ispAlgo;
size_t g_ispAlgoSize;
const uint8_t* g_ispData;
size_t g_ispDataSize;

short int ispEntryPoint();

#endif // INCLUDE_HARDWARE_H
