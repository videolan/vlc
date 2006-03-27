/*****************************************************************************
 * strings.c: String related functions
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
 *          Daniel Stranger <vlc at schmaller dot de>
 *          Rémi Denis-Courmont <rem # videolan org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "vlc_strings.h"

/**
 * Decode URI encoded string
 * \return decoded duplicated string
 */
char *decode_encoded_URI_duplicate( const char *psz )
{
    char *psz_dup = strdup( psz );
    decode_encoded_URI( psz_dup );
    return psz_dup;
}

/**
 * Decode URI encoded string
 * \return nothing
 */
void decode_encoded_URI( char *psz )
{
    unsigned char *in = (unsigned char *)psz, *out = in, c;

    while( ( c = *in++ ) != '\0' )
    {
        switch( c )
        {
            case '%':
            {
                char val[5], *pval = val;
                unsigned long cp;

                switch( c = *in++ )
                {
                    case '\0':
                        return;

                    case 'u':
                    case 'U':
                        if( ( *pval++ = *in++ ) == '\0' )
                            return;
                        if( ( *pval++ = *in++ ) == '\0' )
                            return;
                        c = *in++;

                    default:
                        *pval++ = c;
                        if( ( *pval++ = *in++ ) == '\0' )
                            return;
                        *pval = '\0';
                }

                cp = strtoul( val, NULL, 0x10 );
                if( cp < 0x80 )
                    *out++ = cp;
                else
                if( cp < 0x800 )
                {
                    *out++ = (( cp >>  6)         | 0xc0);
                    *out++ = (( cp        & 0x3f) | 0x80);
                }
                else
                {
                    assert( cp < 0x10000 );
                    *out++ = (( cp >> 12)         | 0xe0);
                    *out++ = (((cp >>  6) & 0x3f) | 0x80);
                    *out++ = (( cp        & 0x3f) | 0x80);
                }
                break;
            }

            case '+':
                *out++ = ' ';
                break;

            default:
                /* Inserting non-ASCII or non-printable characters is unsafe,
                 * and no sane browser will send these unencoded */
                if( ( c < 32 ) || ( c > 127 ) )
                    *out++ = '?';
                else
                    *out++ = c;
        }
    }
    *out = '\0';
}

/**
 * Converts "&lt;", "&gt;" and "&amp;" to "<", ">" and "&"
 * \param string to convert
 */
void resolve_xml_special_chars( char *psz_value )
{
    char *p_pos = psz_value;

    while ( *psz_value )
    {
        if( !strncmp( psz_value, "&lt;", 4 ) )
        {
            *p_pos = '<';
            psz_value += 4;
        }
        else if( !strncmp( psz_value, "&gt;", 4 ) )
        {
            *p_pos = '>';
            psz_value += 4;
        }
        else if( !strncmp( psz_value, "&amp;", 5 ) )
        {
            *p_pos = '&';
            psz_value += 5;
        }
        else if( !strncmp( psz_value, "&quot;", 6 ) )
        {
            *p_pos = '\"';
            psz_value += 6;
        }
        else if( !strncmp( psz_value, "&#039;", 6 ) )
        {
            *p_pos = '\'';
            psz_value += 6;
        }
        else
        {
            *p_pos = *psz_value;
            psz_value++;
        }

        p_pos++;
    }

    *p_pos = '\0';
}

/**
 * Converts '<', '>', '\"', '\'' and '&' to their html entities
 * \param psz_content simple element content that is to be converted
 */
char *convert_xml_special_chars( const char *psz_content )
{
    char *psz_temp = malloc( 6 * strlen( psz_content ) + 1 );
    const char *p_from = psz_content;
    char *p_to   = psz_temp;

    while ( *p_from )
    {
        if ( *p_from == '<' )
        {
            strcpy( p_to, "&lt;" );
            p_to += 4;
        }
        else if ( *p_from == '>' )
        {
            strcpy( p_to, "&gt;" );
            p_to += 4;
        }
        else if ( *p_from == '&' )
        {
            strcpy( p_to, "&amp;" );
            p_to += 5;
        }
        else if( *p_from == '\"' )
        {
            strcpy( p_to, "&quot;" );
            p_to += 6;
        }
        else if( *p_from == '\'' )
        {
            strcpy( p_to, "&#039;" );
            p_to += 6;
        }
        else
        {
            *p_to = *p_from;
            p_to++;
        }
        p_from++;
    }
    *p_to = '\0';

    return psz_temp;
}
