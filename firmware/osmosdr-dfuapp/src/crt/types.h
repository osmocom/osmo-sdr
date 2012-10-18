#ifndef INCLUDE_TYPES_H
#define INCLUDE_TYPES_H

#include <stdint.h>

typedef uint64_t u64;
typedef int64_t s64;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint8_t u8;
typedef int8_t s8;

typedef unsigned int uint;

typedef enum Bool_ {
	False = 0,
	True = 1
} Bool;

#endif // INCLUDE_TYPES_H
