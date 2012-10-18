/* (C) 2011-2012 by Christian Daniel <cd@maintech.de>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "string.h"

size_t strlen(const char* str)
{
	size_t res = 0;
	while(*str++ != '\0')
		res++;
	return res;
}

int strcmp(const char *s1, const char *s2)
{
	s8 res;

	while(1) {
		if((res = *s1 - *s2++) != 0 || !*s1++)
			break;
	}

	return res;
}

int strncmp(const char* s1, const char* s2, size_t n)
{
	s8 res = 0;

	while(n) {
		if((res = *s1 - *s2++) != 0 || !*s1++)
			break;
		n--;
	}

	return res;
}

void* memcpy(void* dest, const void* src, size_t n)
{
	u8* d = (u8*)dest;
	const u8* s = (const u8*)src;

	while(n--)
		*d++ = *s++;
	return dest;
}
