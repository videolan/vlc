/*****************************************************************************
 * strsep.c: BSD strsep() replacement
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

#include <string.h>

char *strsep( char **ppsz_string, const char *psz_delimiters )
{
    char *psz_string = *ppsz_string;
    if( !psz_string )
        return NULL;

    char *p = strpbrk( psz_string, psz_delimiters );
    if( !p )
    {
        *ppsz_string = NULL;
        return psz_string;
    }
    *p++ = '\0';

    *ppsz_string = p;
    return psz_string;
}
