/* debug.h: debugging output functions
 *
 * Copyright (C) 2021 thatlittlegit
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, under version 3 of the License,
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef UU_DEBUG_H
#define UU_DEBUG_H

#include <stdarg.h>
#include <stdio.h>

extern FILE* debug_stream;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#endif
static void
debug(const char* str, ...)
{
	if (debug_stream) {
		va_list list;
		va_start(list, str);
		vfprintf(debug_stream, str, list);
		va_end(list);
	}
}

#endif
