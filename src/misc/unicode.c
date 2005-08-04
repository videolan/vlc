/*****************************************************************************
 * unicode.c: UTF8 <-> locale functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include "charset.h"

/*****************************************************************************
 * EnsureUTF8: replaces invalid/overlong UTF-8 sequences with question marks
 *****************************************************************************
 * Not Todo : convert Latin1 to UTF-8 on the flu
 * It is not possible given UTF-8 needs more space
 *****************************************************************************/
#define isutf8cont( c ) (((c) >= 0x80) && ((c) <= 0xBF)) 
char *EnsureUTF8( char *str )
{
    unsigned char *ptr, c;

    ptr = (unsigned char *)str;
    while( (c = *ptr) != '\0' )
    {
        /* US-ASCII, 1 byte */
        if( ( ( c >= 0x20 ) && ( c <= 0x7F ) )
         || ( c == 0x09 ) || ( c == 0x0A ) || ( c == 0x0D ) )
        {
            ptr++; /* OK */
        }
        else
        /* 2 bytes */
        if( ( c >= 0xC2 ) && ( c <= 0xDF ) )
        {
            c = ptr[1];
            if( isutf8cont( c ) )
                ptr += 2; /* OK */
            else
                *ptr++ = '?'; /* invalid */
        }
        else
        /* 3 bytes */
        if( c == 0xE0 )
        {
            c = ptr[1];
            if( ( c >= 0xA0 ) && ( c <= 0xBF ) )
            {
                c = ptr[2];
                if( isutf8cont( c ) )
                    ptr += 3; /* OK */
                else
                    *ptr++ = '?';
            }
            else
                *ptr++ = '?';
        }
        else
        if( ( ( c >= 0xE1 ) && ( c <= 0xEC ) ) || ( c == 0xEC )
         || ( c == 0xEE ) || ( c == 0xEF ) )
        {
            c = ptr[1];
            if( isutf8cont( c ) )
            {
                c = ptr[2];
                if( isutf8cont( c ) )
                    ptr += 3; /* OK */
                else
                    *ptr++ = '?';
            }
            else
                *ptr++ = '?';
        }
        else
        if( c == 0xED )
        {
            c = ptr[1];
            if( ( c >= 0x80 ) && ( c <= 0x9F ) )
            {
                c = ptr[2];
                if( isutf8cont( c ) )
                    ptr += 3; /* OK */
                else
                    *ptr++ = '?';
            }
            else
                *ptr++ = '?';
        }
        else
        /* 4 bytes */
        if( c == 0xF0 )
        {
            c = ptr[1];
            if( ( c >= 0x90 ) && ( c <= 0xBF ) )
            {
                c = ptr[2];
                if( isutf8cont( c ) )
                {
                    c = ptr[3];
                    if( isutf8cont( c ) )
                        ptr += 4; /* OK */
                    else
                        *ptr++ = '?';
                }
                else
                    *ptr++ = '?';
            }
            else
                *ptr++ = '?';
        }
        else
        if( ( c >= 0xF1 ) && ( c <= 0xF3 ) )
        {
            c = ptr[1];
            if( isutf8cont( c ) )
            {
                c = ptr[2];
                if( isutf8cont( c ) )
                {
                    c = ptr[3];
                    if( isutf8cont( c ) )
                        ptr += 4; /* OK */
                    else
                        *ptr++ = '?';
                }
                else
                    *ptr++ = '?';
            }
            else
                *ptr++ = '?';
        }
        else
        if( c == 0xF4 )
        {
            c = ptr[1];
            if( ( c >= 0x80 ) && ( c <= 0x8F ) )
            {
                c = ptr[2];
                if( isutf8cont( c ) )
                {
                    c = ptr[3];
                    if( isutf8cont( c ) )
                        ptr += 4; /* OK */
                    else
                        *ptr++ = '?';
                }
                else
                    *ptr++ = '?';
            }
            else
                *ptr++ = '?';
        }
        else
            *ptr++ = '?';
    }

    return str;
}
