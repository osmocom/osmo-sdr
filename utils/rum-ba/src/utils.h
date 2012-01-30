#ifndef INCLUDE_UTILS_H
#define INCLUDE_UTILS_H

#include <stdint.h>

uint64_t getTickCount();
void* loadFile(const char* filename, size_t* size);

#ifndef WINDOWS
typedef int HANDLE;
#define INVALID_HANDLE_VALUE (-1)
#else
#include <windows.h>
#endif // WINDOWS

#endif // INCLUDE_UTILS_H
