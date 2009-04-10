/*****************************************************************************
 * strcasecmp.c: POSIX strcasecmp() replacement
 *****************************************************************************
 * Copyright © 1998-2008 the VideoLAN project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <assert.h>

int strcasecmp (const char *s1, const char *s2)
{
#ifdef HAVE_STRICMP
    return stricmp (s1, s2);
#else
    for (size_t i = 0;; i++)
    {
        int d = tolower (s1[i]) - tolower (s2[i]);
        if (d || !s1[i])
            return d;
        assert (s2[i]);
    }
#endif
}
