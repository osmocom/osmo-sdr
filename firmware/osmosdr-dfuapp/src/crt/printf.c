/*
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include "stdio.h"
#include "types.h"

void printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[PRINTF_BUFSIZE];

	//format the string
	va_start (args, fmt);
	i = vsprintf(printbuffer, fmt, args);
	va_end (args);

	//print the string
	puts(printbuffer);
}

void dprintf(const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[PRINTF_BUFSIZE];

	//format the string
	va_start (args, fmt);
	i = vsprintf(printbuffer, fmt, args);
	va_end (args);

	//print the string
	dputs(printbuffer);
}
