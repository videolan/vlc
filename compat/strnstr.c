/*****************************************************************************
 * strnstr.c: BSD strnstr() replacement
 *****************************************************************************
 * Copyright © 2015 VLC authors and VideoLAN
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

#include <string.h>
#include <assert.h>

char * strnstr (const char *haystack, const char *needle, size_t len)
{
    assert(needle != NULL);

    const size_t i = strlen(needle);
    if (i == 0) /* corner case (if haystack is NULL, memcmp not allowed) */
        return (char *)haystack;

    if( len < i )
      return NULL;

    size_t count = len - i;

    do
    {
      if( memcmp(haystack, needle, i) == 0 )
        return (char*) haystack;
      haystack++;
    }
    while(count--);

    return NULL;
}
