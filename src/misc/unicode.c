/*****************************************************************************
 * unicode.c: UTF8 <-> locale functions
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
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

#include <assert.h>

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#  include <dirent.h>
#endif
#ifdef UNDER_CE
#  include <tchar.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifndef HAVE_LSTAT
# define lstat( a, b ) stat(a, b)
#endif

#ifdef __APPLE__
/* Define this if the OS always use UTF-8 internally */
# define ASSUME_UTF8 1
#endif

#if !(defined (WIN32) || defined (UNDER_CE) || defined (ASSUME_UTF8))
# define USE_ICONV 1
#endif

#if defined (USE_ICONV) && !defined (HAVE_ICONV)
# error No UTF8 charset conversion implemented on this platform!
#endif



#ifdef USE_ICONV
static struct {
    vlc_iconv_t hd;
    vlc_mutex_t lock;
} from_locale, to_locale;
#endif

void LocaleInit( vlc_object_t *p_this )
{
#ifdef USE_ICONV
    char *psz_charset;

    if( vlc_current_charset( &psz_charset ) )
        /* UTF-8 */
        from_locale.hd = to_locale.hd = (vlc_iconv_t)(-1);
    else
    {
        /* not UTF-8 */
        char *psz_conv = psz_charset;

        /*
         * Still allow non-ASCII characters when the locale is not set.
         * Western Europeans are being favored for historical reasons.
         */
        psz_conv = strcmp( psz_charset, "ASCII" )
                ? psz_charset : "ISO-8859-1";

        vlc_mutex_init( p_this, &from_locale.lock );
        vlc_mutex_init( p_this, &to_locale.lock );
        from_locale.hd = vlc_iconv_open( "UTF-8", psz_charset );
        to_locale.hd = vlc_iconv_open( psz_charset, "UTF-8" );
    }

    free( psz_charset );

    assert( (from_locale.hd == (vlc_iconv_t)(-1))
            == (to_locale.hd == (vlc_iconv_t)(-1)) );
#else
    (void)p_this;
#endif
}

void LocaleDeinit( void )
{
#ifdef USE_ICONV
    if( to_locale.hd != (vlc_iconv_t)(-1) )
    {
        vlc_iconv_close( to_locale.hd );
        vlc_mutex_destroy( &to_locale.lock );
    }

    if( from_locale.hd != (vlc_iconv_t)(-1) )
    {
        vlc_iconv_close( from_locale.hd );
        vlc_mutex_destroy( &from_locale.lock );
    }
#endif
}

#if defined (WIN32) || defined (UNDER_CE)
static char *MB2MB( const char *string, UINT fromCP, UINT toCP )
{
    char *out;
    int ilen = strlen( string ), olen = (4 / sizeof (wchar_t)) * ilen + 1;
    wchar_t wide[olen];

    ilen = MultiByteToWideChar( fromCP, 0, string, ilen + 1, wide, olen );
    if( ilen == 0 )
        return NULL;

    olen = 4 * ilen + 1;
    out = malloc( olen );

    olen = WideCharToMultiByte( toCP, 0, wide, ilen, out, olen, NULL, NULL );
    if( olen == 0 )
    {
        free( out );
        return NULL;
    }
    return realloc( out, olen );
}
#endif

/*****************************************************************************
 * FromLocale: converts a locale string to UTF-8
 *****************************************************************************/
char *FromLocale( const char *locale )
{
    if( locale == NULL )
        return NULL;

#if !(defined WIN32 || defined (UNDER_CE))
# ifdef USE_ICONV
    if( from_locale.hd != (vlc_iconv_t)(-1) )
    {
        char *iptr = (char *)locale, *output, *optr;
        size_t inb, outb;

        /*
         * We are not allowed to modify the locale pointer, even if we cast it
         * to non-const.
         */
        inb = strlen( locale );
        /* FIXME: I'm not sure about the value for the multiplication
         * (for western people, multiplication by 3 (Latin9) is needed).
         * While UTF-8 could reach 6 bytes, no existing code point exceeds
         * 4 bytes. */
        outb = inb * 4 + 1;

        optr = output = malloc( outb );

        vlc_mutex_lock( &from_locale.lock );
        vlc_iconv( from_locale.hd, NULL, NULL, NULL, NULL );

        while( vlc_iconv( from_locale.hd, &iptr, &inb, &optr, &outb )
               == (size_t)-1 )
        {
            *optr++ = '?';
            outb--;
            iptr++;
            inb--;
            vlc_iconv( from_locale.hd, NULL, NULL, NULL, NULL );
        }
        vlc_mutex_unlock( &from_locale.lock );
        *optr = '\0';

        assert (inb == 0);
        assert (*iptr == '\0');
        assert (*optr == '\0');
        assert (strlen( output ) == (size_t)(optr - output));
        return realloc( output, optr - output + 1 );
    }
# endif /* USE_ICONV */
    return (char *)locale;
#else /* WIN32 */
    return MB2MB( locale, CP_ACP, CP_UTF8 );
#endif
}

char *FromLocaleDup( const char *locale )
{
#if defined (ASSUME_UTF8)
    return strdup( locale );
#else
# ifdef USE_ICONV
    if (from_locale.hd == (vlc_iconv_t)(-1))
        return strdup( locale );
# endif
    return FromLocale( locale );
#endif
}


/*****************************************************************************
 * ToLocale: converts an UTF-8 string to locale
 *****************************************************************************/
char *ToLocale( const char *utf8 )
{
    if( utf8 == NULL )
        return NULL;

#if !(defined (WIN32) || defined (UNDER_CE))
# ifdef USE_ICONV
    if( to_locale.hd != (vlc_iconv_t)(-1) )
    {
        char *iptr = (char *)utf8, *output, *optr;
        size_t inb, outb;

        /*
        * We are not allowed to modify the locale pointer, even if we cast it
        * to non-const.
        */
        inb = strlen( utf8 );
        /* FIXME: I'm not sure about the value for the multiplication
        * (for western people, multiplication is not needed) */
        outb = inb * 2 + 1;

        optr = output = malloc( outb );
        vlc_mutex_lock( &to_locale.lock );
        vlc_iconv( to_locale.hd, NULL, NULL, NULL, NULL );

        while( vlc_iconv( to_locale.hd, &iptr, &inb, &optr, &outb )
               == (size_t)-1 )
        {
            *optr++ = '?'; /* should not happen, and yes, it sucks */
            outb--;
            iptr++;
            inb--;
            vlc_iconv( to_locale.hd, NULL, NULL, NULL, NULL );
        }
        vlc_mutex_unlock( &to_locale.lock );
        *optr = '\0';

        assert (inb == 0);
        assert (*iptr == '\0');
        assert (*optr == '\0');
        assert (strlen( output ) == (size_t)(optr - output));
        return realloc( output, optr - output + 1 );
    }
# endif /* USE_ICONV */
    return (char *)utf8;
#else /* WIN32 */
    return MB2MB( utf8, CP_UTF8, CP_ACP );
#endif
}

void LocaleFree( const char *str )
{
#ifdef USE_ICONV
    if( to_locale.hd == (vlc_iconv_t)(-1) )
        return;
#endif

#ifndef ASSUME_UTF8
    if( str != NULL )
        free( (char *)str );
#endif
}

/*****************************************************************************
 * utf8_fopen: Calls fopen() after conversion of file name to OS locale
 *****************************************************************************/
FILE *utf8_fopen( const char *filename, const char *mode )
{
#if !(defined (WIN32) || defined (UNDER_CE))
    const char *local_name = ToLocale( filename );

    if( local_name != NULL )
    {
        FILE *stream = fopen( local_name, mode );
        LocaleFree( local_name );
        return stream;
    }
    else
        errno = ENOENT;
    return NULL;
#else
    wchar_t wpath[MAX_PATH];
    wchar_t wmode[4];

    if( !MultiByteToWideChar( CP_UTF8, 0, filename, -1, wpath, MAX_PATH - 1)
     || !MultiByteToWideChar( CP_ACP, 0, mode, -1, wmode, 3 ) )
    {
        errno = ENOENT;
        return NULL;
    }

    return _wfopen( wpath, wmode );
#endif
}

/*****************************************************************************
 * utf8_mkdir: Calls mkdir() after conversion of file name to OS locale
 *****************************************************************************/
int utf8_mkdir( const char *dirname )
{
#if defined (UNDER_CE) || defined (WIN32)
    wchar_t wname[MAX_PATH];
    char mod[MAX_PATH];
    int i;

    /* Convert '/' into '\' */
    for( i = 0; *dirname; i++ )
    {
        if( i == MAX_PATH )
            return -1; /* overflow */

        if( *dirname == '/' )
            mod[i] = '\\';
        else
            mod[i] = *dirname;
        dirname++;

    }
    mod[i] = 0;

    if( MultiByteToWideChar( CP_UTF8, 0, mod, -1, wname, MAX_PATH ) == 0 )
    {
        errno = ENOENT;
        return -1;
    }

    if( CreateDirectoryW( wname, NULL ) == 0 )
    {
        if( GetLastError( ) == ERROR_ALREADY_EXISTS )
            errno = EEXIST;
        errno = ENOENT;
        return -1;
    }
    return 0;
#else
    char *locname = ToLocale( dirname );
    int res;

    if( locname == NULL )
    {
        errno = ENOENT;
        return -1;
    }
    res = mkdir( locname, 0755 );

    LocaleFree( locname );
    return res;
#endif
}


void *utf8_opendir( const char *dirname )
{
    const char *local_name = ToLocale( dirname );

    if( local_name != NULL )
    {
        DIR *dir = opendir( local_name );
        LocaleFree( local_name );
        return dir;
    }
    else
        errno = ENOENT;
    return NULL;
}

const char *utf8_readdir( void *dir )
{
    struct dirent *ent;

    ent = readdir( (DIR *)dir );
    if( ent == NULL )
        return NULL;

    return FromLocale( ent->d_name );
}


static int utf8_statEx( const char *filename, void *buf,
                        vlc_bool_t deref )
{
#ifdef HAVE_SYS_STAT_H
    const char *local_name = ToLocale( filename );

    if( local_name != NULL )
    {
        int res = deref ? stat( local_name, (struct stat *)buf )
                       : lstat( local_name, (struct stat *)buf );
        LocaleFree( local_name );
        return res;
    }
    errno = ENOENT;
#endif
    return -1;
}


int utf8_stat( const char *filename, void *buf)
{
    return utf8_statEx( filename, buf, VLC_TRUE );
}

int utf8_lstat( const char *filename, void *buf)
{
    return utf8_statEx( filename, buf, VLC_FALSE );
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
