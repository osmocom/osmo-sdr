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

#include "stdio.h"
#include "../driver/dbgio.h"

void puts(const char* str)
{
	while(*str != '\0') {
		if(*str == '\n')
			dbgio_putChar('\r');
		dbgio_putChar(*str++);
	}
}

void dputs(const char* str)
{
	while(*str != '\0') {
		if(*str == '\n')
			dbgio_putCharDirect('\r');
		dbgio_putCharDirect(*str++);
	}
}
