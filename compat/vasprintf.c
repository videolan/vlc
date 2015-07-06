/*****************************************************************************
 * vasprintf.c: GNU vasprintf() replacement
 *****************************************************************************
 * Copyright © 1998-2009 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

int vasprintf (char **strp, const char *fmt, va_list ap)
{
    va_list args;
    int len;

    va_copy (args, ap);
    len = vsnprintf (NULL, 0, fmt, args);
    va_end (args);

    char *str = malloc (len + 1);
    if (str != NULL)
    {
        int len2;

        va_copy (args, ap);
        len2 = vsprintf (str, fmt, args);
        assert (len2 == len);
        va_end (args);
    }
    else
    {
        len = -1;
#ifndef NDEBUG
        str = (void *)(intptr_t)0x41414141; /* poison */
#endif
    }
    *strp = str;
    return len;
}
