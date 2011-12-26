#ifndef INCLUDE_UTILS_H
#define INCLUDE_UTILS_H

#include <stdint.h>

uint64_t getTickCount();
void* loadFile(const char* filename, size_t* size);

#endif // INCLUDE_UTILS_H
