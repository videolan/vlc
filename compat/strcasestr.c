/*****************************************************************************
 * strcasestr.c: GNU strcasestr() replacement
 *****************************************************************************
 * Copyright © 2002-2011 VLC authors and VideoLAN
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
#include <ctype.h>
#include <assert.h>

char *strcasestr (const char *psz_big, const char *psz_little)
{
    char *p_pos = (char *)psz_big;

    if( !*psz_little ) return p_pos;

    while( *p_pos )
    {
        if( toupper( (unsigned char)*p_pos ) == toupper( (unsigned char)*psz_little ) )
        {
            char *cur1 = p_pos + 1;
            char *cur2 = (char *)psz_little + 1;
            while( *cur1 && *cur2
             && toupper( (unsigned char)*cur1 ) == toupper( (unsigned char)*cur2 ) )
            {
                cur1++;
                cur2++;
            }
            if( !*cur2 ) return p_pos;
        }
        p_pos++;
    }
    return NULL;
}
