/*****************************************************************************
 * strlcpy.c: BSD strlcpy() replacement
 *****************************************************************************
 * Copyright © 2006, 2009 Rémi Denis-Courmont
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

#include <stddef.h>

/**
 * Copy a string to a sized buffer. The result is always nul-terminated
 * (contrary to strncpy()).
 *
 * @param dest destination buffer
 * @param src string to be copied
 * @param len maximum number of characters to be copied plus one for the
 * terminating nul.
 *
 * @return strlen(src)
 */
size_t strlcpy (char *tgt, const char *src, size_t bufsize)
{
    size_t length;

    for (length = 1; (length < bufsize) && *src; length++)
        *tgt++ = *src++;

    if (bufsize)
        *tgt = '\0';

    while (*src++)
        length++;

    return length - 1;
}
