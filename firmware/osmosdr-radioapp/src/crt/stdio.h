#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

#include <stdarg.h>

#define PRINTF_BUFSIZE 96

void printf(const char *fmt, ...);
void dprintf(const char *fmt, ...);
void vprintf(const char *fmt, va_list args);
int vsprintf(char *buf, const char *fmt, va_list args);
void puts(const char* str);
void dputs(const char* str);

#endif // INCLUDE_STDIO_H
