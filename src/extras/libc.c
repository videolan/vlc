/*****************************************************************************
 * libc.c: Extra libc function for some systems.
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: libc.c,v 1.1 2002/11/10 23:41:53 sam Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <string.h>                                              /* strdup() */
#include <stdlib.h>

#include <vlc/vlc.h>

/*****************************************************************************
 * getenv: just in case, but it should never be called
 *****************************************************************************/
#ifndef HAVE_GETENV
char *getenv( const char *name )
{
    return NULL;
}
#endif

/*****************************************************************************
 * strdup: returns a malloc'd copy of a string 
 *****************************************************************************/
#ifndef HAVE_STRDUP
char *strdup( const char *string )
{
    return strndup( string, strlen( string ) );
}
#endif

/*****************************************************************************
 * strndup: returns a malloc'd copy of at most n bytes of string 
 * Does anyone know whether or not it will be present in Jaguar?
 *****************************************************************************/
#ifndef HAVE_STRDUP
char *strndup( const char *string, size_t n )
{
    char *psz;
    size_t len = strlen( string );

    len = __MIN( len, n ); 
    psz = (char*)malloc( len + 1 );

    if( psz != NULL )
    {
        memcpy( (void*)psz, (const void*)string, len );
        psz[ len ] = 0;
    }

    return( psz );
}
#endif

/*****************************************************************************
 * strcasecmp: compare two strings ignoring case
 *****************************************************************************/
#if !defined( HAVE_STRCASECMP ) && !defined( HAVE_STRICMP )
int strcasecmp( const char *s1, const char *s2 )
{
    int i_delta = 0;

    while( !i_delta && *s1 && *s2 )
    {
        i_delta = *s1 - *s2;

        if( *s1 >= 'A' && *s1 <= 'Z' )
        {
            i_delta -= 'A' - 'a';
        }

        if( *s2 >= 'A' && *s2 <= 'Z' )
        {
            i_delta += 'A' - 'a';
        }
    }

    return i_delta;
}
#endif

/*****************************************************************************
 * strncasecmp: compare n chars from two strings ignoring case
 *****************************************************************************/
#if !defined( HAVE_STRNCASECMP ) && !defined( HAVE_STRNICMP )
int strncasecmp( const char *s1, const char *s2, size_t n )
{
    int i_delta = 0;

    while( n-- && !i_delta && *s1 )
    {
        i_delta = *s1 - *s2;

        if( *s1 >= 'A' && *s1 <= 'Z' )
        {
            i_delta -= 'A' - 'a';
        }

        if( *s2 >= 'A' && *s2 <= 'Z' )
        {
            i_delta += 'A' - 'a';
        }
    }

    return i_delta;
}
#endif

/*****************************************************************************
 * atof: convert a string to a double.
 *****************************************************************************/
#ifndef HAVE_ATOF
double atof( const char *nptr )
{ 
    double f_result;
    wchar_t *psz_tmp;
    int i_len = strlen( nptr ) + 1;
  
    psz_tmp = malloc( i_len * sizeof(wchar_t) );
    MultiByteToWideChar( CP_ACP, 0, nptr, -1, psz_tmp, i_len );
    f_result = wcstod( psz_tmp, NULL );
    free( psz_tmp );

    return f_result;
}
#endif

/*****************************************************************************
 * lseek: reposition read/write file offset.
 *****************************************************************************
 * FIXME: this cast sucks!
 *****************************************************************************/
#if !defined( HAVE_LSEEK )
off_t lseek( int fildes, off_t offset, int whence )
{
    return SetFilePointer( (HANDLE)fildes, offset, NULL, whence );
}
#endif

