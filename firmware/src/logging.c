#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>


void logp2(int subsys, unsigned int level, char *file,
	   int line, int cont, const char *format, ...)
{
	va_list ap;
	printf("%u/%u/%s:%u: ", subsys, level, file, line);

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
