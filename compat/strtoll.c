/*****************************************************************************
 * strtoll.c: C strtoll() replacement
 *****************************************************************************
 * Copyright © 1998-2010 VLC authors and VideoLAN
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

#include <stdlib.h>
#include <string.h>

long long int strtoll( const char *nptr, char **endptr, int base )
{
    long long i_value = 0;
    int sign = 1, newbase = base ? base : 10;

    nptr += strspn( nptr, "\t " );
    if( *nptr == '-' )
    {
        sign = -1;
        nptr++;
    }

    /* Try to detect base */
    if( *nptr == '0' )
    {
        newbase = 8;
        nptr++;

        if( *nptr == 'x' )
        {
            newbase = 16;
            nptr++;
        }
    }

    if( base && newbase != base )
    {
        if( endptr ) *endptr = (char *)nptr;
        return i_value;
    }

    switch( newbase )
    {
        case 10:
            while( *nptr >= '0' && *nptr <= '9' )
            {
                i_value *= 10;
                i_value += ( *nptr++ - '0' );
            }
            if( endptr ) *endptr = (char *)nptr;
            break;

        case 16:
            while( (*nptr >= '0' && *nptr <= '9') ||
                   (*nptr >= 'a' && *nptr <= 'f') ||
                   (*nptr >= 'A' && *nptr <= 'F') )
            {
                int i_valc = 0;
                if(*nptr >= '0' && *nptr <= '9') i_valc = *nptr - '0';
                else if(*nptr >= 'a' && *nptr <= 'f') i_valc = *nptr - 'a' +10;
                else if(*nptr >= 'A' && *nptr <= 'F') i_valc = *nptr - 'A' +10;
                i_value *= 16;
                i_value += i_valc;
                nptr++;
            }
            if( endptr ) *endptr = (char *)nptr;
            break;

        default:
            i_value = strtol( nptr, endptr, newbase );
            break;
    }

    return i_value * sign;
}
