#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H

#include <stddef.h>
#include "types.h"

size_t strlen(const char* str);
int strcmp(const char *s1, const char *s2);
int strncmp(const char* s1, const char* s2, size_t n);
void* memcpy(void* dest, const void* src, size_t n);

long strtol(const char *nptr, char **endptr, int ibase);
unsigned long strtoul(const char *nptr, char **endptr, int ibase);
long atol(const char *nptr);
int atoi(const char *nptr);
s64 strtoll(const char *nptr, char **endptr, int ibase);

#endif // INCLUDE_STRING_H
