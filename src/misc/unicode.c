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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include "charset.h"
#include <stdio.h>

/*****************************************************************************
 * vlc_fopen: Calls fopen() after conversion of file name to OS locale
 *****************************************************************************/
FILE *vlc_fopen( const char *filename, const char *mode )
{
    const char *local_name = ToLocale( filename );

    if( local_name != NULL )
    {
        FILE *stream = fopen( local_name, mode );
        LocaleFree( local_name );
        return stream;
    }
    return NULL;
}

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

/**********************************************************************
 * UTF32toUTF8: converts an array from UTF-32 to UTF-8
 *********************************************************************/
char *UTF32toUTF8( const wchar_t *src, size_t len, size_t *newlen )
{
    char *res, *out;

    /* allocate memory */
    out = res = (char *)malloc( 6 * len );
    if( res == NULL )
        return NULL;

    while( len > 0 )
    {
        uint32_t uv = *src++;
        len--;

        if( uv < 0x80 )
        {
            *out++ = uv;
            continue;
        }
        else
        if( uv < 0x800 )
        {
            *out++ = (( uv >>  6)         | 0xc0);
            *out++ = (( uv        & 0x3f) | 0x80);
            continue;
        }
        else
        if( uv < 0x10000 )
        {
            *out++ = (( uv >> 12)         | 0xe0);
            *out++ = (((uv >>  6) & 0x3f) | 0x80);
            *out++ = (( uv        & 0x3f) | 0x80);
            continue;
        }
        else
        {
            *out++ = (( uv >> 18)         | 0xf0);
            *out++ = (((uv >> 12) & 0x3f) | 0x80);
            *out++ = (((uv >>  6) & 0x3f) | 0x80);
            *out++ = (( uv        & 0x3f) | 0x80);
            continue;
        }
    }
    len = out - res;
    res = realloc( res, len );
    if( newlen != NULL )
        *newlen = len;
    return res;
}

/**********************************************************************
 * FromUTF32: converts an UTF-32 string to UTF-8
 **********************************************************************
 * The result must be free()'d. NULL on error.
 *********************************************************************/
char *FromUTF32( const wchar_t *src )
{
    size_t len;
    const wchar_t *in;

    /* determine the size of the string */
    for( len = 1, in = src; GetWBE( in ); len++ )
        in++;

    return UTF32toUTF8( src, len, NULL );
}
