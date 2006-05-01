/*****************************************************************************
 * unicode.c: Unicode <-> locale functions
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
 *
 * UTF16toUTF8() adapted from Perl 5 (also GPL'd)
 * Copyright (C) 1998-2002, Larry Wall
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
#include <stdarg.h>
#include <stdlib.h>
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

#ifndef ASSUME_UTF8
# if defined (HAVE_ICONV)
/* libiconv is more powerful than Win32 API (it has translit) */
#  define USE_ICONV 1
# elif defined (WIN32) || defined (UNDER_CE)
#  define USE_MB2MB 1
# else
#  error No UTF8 charset conversion implemented on this platform!
# endif
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
        char psz_buf[strlen( psz_charset ) + sizeof( "//translit" )];
        const char *psz_conv;

        /*
         * Still allow non-ASCII characters when the locale is not set.
         * Western Europeans are being favored for historical reasons.
         */
        if( strcmp( psz_charset, "ASCII" ) )
        {
            sprintf( psz_buf, "%s//translit", psz_charset );
            psz_conv = psz_buf;
        }
        else
            psz_conv = "ISO-8859-1//translit";

        vlc_mutex_init( p_this, &from_locale.lock );
        vlc_mutex_init( p_this, &to_locale.lock );
        from_locale.hd = vlc_iconv_open( "UTF-8", psz_conv );
        to_locale.hd = vlc_iconv_open( psz_conv, "UTF-8" );
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

#ifdef USE_MB2MB
static char *MB2MB( const char *string, UINT fromCP, UINT toCP )
{
    char *out;
    wchar_t *wide;
    int len;

    len = MultiByteToWideChar( fromCP, 0, string, -1, NULL, 0 );
    assert( len > 0 );
    wide = (wchar_t *)malloc (len * sizeof (wchar_t));
    if( wide == NULL )
        return NULL;

    MultiByteToWideChar( fromCP, 0, string, -1, wide, len );
    len = WideCharToMultiByte( toCP, 0, wide, -1, NULL, 0, NULL, NULL );
    assert( len > 0 );
    out = malloc( len );

    WideCharToMultiByte( toCP, 0, wide, -1, out, len, NULL, NULL );
    free( wide );
    return out;
}
#endif

/**
 * FromLocale: converts a locale string to UTF-8
 *
 * @param locale nul-terminated string to be converted
 *
 * @return a nul-terminated UTF-8 string, or NULL in case of error.
 * To avoid memory leak, you have to pass the result to LocaleFree()
 * when it is no longer needed.
 */
char *FromLocale( const char *locale )
{
    if( locale == NULL )
        return NULL;

#ifndef USE_MB2MB
# ifdef USE_ICONV
    if( from_locale.hd != (vlc_iconv_t)(-1) )
    {
        const char *iptr = locale;
        size_t inb = strlen( locale );
        size_t outb = inb * 6 + 1;
        char output[outb], *optr = output;

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
        return strdup( output );
    }
# endif /* USE_ICONV */
    return (char *)locale;
#else /* MB2MB */
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


/**
 * ToLocale: converts a UTF-8 string to local system encoding.
 *
 * @param utf8 nul-terminated string to be converted
 *
 * @return a nul-terminated string, or NULL in case of error.
 * To avoid memory leak, you have to pass the result to LocaleFree()
 * when it is no longer needed.
 */
char *ToLocale( const char *utf8 )
{
    if( utf8 == NULL )
        return NULL;

#ifndef USE_MB2MB
# ifdef USE_ICONV
    if( to_locale.hd != (vlc_iconv_t)(-1) )
    {
        const char *iptr = utf8;
        size_t inb = strlen( utf8 );
        /* FIXME: I'm not sure about the value for the multiplication
        * (for western people, multiplication is not needed) */
        size_t outb = inb * 2 + 1;

        char output[outb], *optr = output;

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
        return strdup( output );
    }
# endif /* USE_ICONV */
    return (char *)utf8;
#else /* MB2MB */
    return MB2MB( utf8, CP_UTF8, CP_ACP );
#endif
}

char *ToLocaleDup( const char *utf8 )
{
#if defined (ASSUME_UTF8)
    return strdup( utf8 );
#else
# ifdef USE_ICONV
    if (to_locale.hd == (vlc_iconv_t)(-1))
        return strdup( utf8 );
# endif
    return ToLocale( utf8 );
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

/**
 * utf8_fopen: Calls fopen() after conversion of file name to OS locale
 */
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
    wchar_t wpath[MAX_PATH + 1];
    size_t len = strlen( mode ) + 1;
    wchar_t wmode[len];

    if( !MultiByteToWideChar( CP_UTF8, 0, filename, -1, wpath, MAX_PATH )
     || !MultiByteToWideChar( CP_ACP, 0, mode, len, wmode, len ) )
    {
        errno = ENOENT;
        return NULL;
    }
    wpath[MAX_PATH] = L'\0';

    /* retrieve OS version */
    if( GetVersion() < 0x80000000 )
    {
        /* for Windows NT and above */
        /*
         * fopen() cannot open files with non-“ANSI” characters on Windows.
         * We use _wfopen() instead. Same thing for mkdir() and stat().
         */
        return _wfopen( wpath, wmode );
    }
    else
    {
        /* for Windows Me/98/95 */
        /* we use GetShortFileNameW to get the DOS 8.3 version of the file we need to open */
        char spath[MAX_PATH + 1];
        if( GetShortPathNameW( wpath, spath, MAX_PATH ) )
        {
            fprintf( stderr, "fopen path: %s -> %s\n", wpath, spath );
            return fopen( spath, wmode );
        }
        fprintf( stderr, "GetShortPathName for %s failed\n", wpath );
        errno = ENOENT;
        return NULL;
    }
#endif
}

/**
 * utf8_mkdir: Calls mkdir() after conversion of file name to OS locale
 */
int utf8_mkdir( const char *dirname )
{
#if defined (UNDER_CE) || defined (WIN32)
    wchar_t wname[MAX_PATH + 1];
    char mod[MAX_PATH + 1];
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
    wname[MAX_PATH] = L'\0';

    if( CreateDirectoryW( wname, NULL ) == 0 )
    {
        if( GetLastError( ) == ERROR_ALREADY_EXISTS )
            errno = EEXIST;
        else
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
        DIR *dir = vlc_opendir_wrapper( local_name );
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

    ent = vlc_readdir_wrapper( (DIR *)dir );
    if( ent == NULL )
        return NULL;

    return FromLocale( ent->d_name );
}

static int dummy_select( const char *str )
{
    (void)str;
    return 1;
}

int utf8_scandir( const char *dirname, char ***namelist,
                  int (*select)( const char * ),
                  int (*compar)( const char **, const char ** ) )
{
    DIR *dir = utf8_opendir( dirname );

    if( select == NULL )
        select = dummy_select;

    if( dir == NULL )
        return -1;
    else
    {
        char **tab = NULL;
        const char *entry;
        unsigned num = 0;

        while( ( entry = utf8_readdir( dir ) ) != NULL )
        {
            char **newtab;
            char *utf_entry = strdup( entry );
            LocaleFree( entry );
            if( utf_entry == NULL )
                goto error;

            if( !select( utf_entry ) )
            {
                free( utf_entry );
                continue;
            }

            newtab = realloc( tab, sizeof( char * ) * (num + 1) );
            if( newtab == NULL )
            {
                free( utf_entry );
                goto error;
            }
            tab = newtab;
            tab[num++] = utf_entry;
        }
        vlc_closedir_wrapper( dir );

        if( compar != NULL )
            qsort( tab, num, sizeof( tab[0] ),
                   (int (*)( const void *, const void *))compar );

        *namelist = tab;
        return num;

    error:{
        unsigned i;

        for( i = 0; i < num; i++ )
            free( tab[i] );
        if( tab != NULL )
            free( tab );
        return -1;}
    }
}


static int utf8_statEx( const char *filename, void *buf,
                        vlc_bool_t deref )
{
#if !(defined (WIN32) || defined (UNDER_CE))
# ifdef HAVE_SYS_STAT_H
    const char *local_name = ToLocale( filename );

    if( local_name != NULL )
    {
        int res = deref ? stat( local_name, (struct stat *)buf )
                       : lstat( local_name, (struct stat *)buf );
        LocaleFree( local_name );
        return res;
    }
    errno = ENOENT;
# endif
    return -1;
#else
    wchar_t wpath[MAX_PATH + 1];

    if( !MultiByteToWideChar( CP_UTF8, 0, filename, -1, wpath, MAX_PATH ) )
    {
        errno = ENOENT;
        return -1;
    }
    wpath[MAX_PATH] = L'\0';

    /* retrieve Windows OS version */
    if( GetVersion() < 0x80000000 )
    {
        /* for Windows NT and above */
        return _wstati64( wpath, (struct _stati64 *)buf );
    }
    else
    {
        /* for Windows Me/98/95 */
        /* we use GetShortFileNameW to get the DOS 8.3 version */
        char spath[MAX_PATH + 1];
        if( GetShortPathNameW( wpath, spath, MAX_PATH ) )
        {
            fprintf( stderr, "stati path: %s -> %s\n", wpath, spath );
            return _stati64( spath, (struct _stati64 *)buf );
        }
        fprintf( stderr, "GetShortPathName for %s failed\n", wpath );
        errno = ENOENT;
        return -1;
    }
#endif
}


int utf8_stat( const char *filename, void *buf)
{
    return utf8_statEx( filename, buf, VLC_TRUE );
}

int utf8_lstat( const char *filename, void *buf)
{
    return utf8_statEx( filename, buf, VLC_FALSE );
}

/**
 * utf8_*printf: *printf with conversion from UTF-8 to local encoding
 */
static int utf8_vasprintf( char **str, const char *fmt, va_list ap )
{
    char *utf8;
    int res = vasprintf( &utf8, fmt, ap );
    if( res == -1 )
        return -1;

    *str = ToLocaleDup( utf8 );
    free( utf8 );
    return res;
}

static int utf8_vfprintf( FILE *stream, const char *fmt, va_list ap )
{
    char *str;
    int res = utf8_vasprintf( &str, fmt, ap );
    if( res == -1 )
        return -1;

    fputs( str, stream );
    free( str );
    return res;
}

int utf8_fprintf( FILE *stream, const char *fmt, ... )
{
    va_list ap;
    int res;

    va_start( ap, fmt );
    res = utf8_vfprintf( stream, fmt, ap );
    va_end( ap );
    return res;
}


static char *CheckUTF8( char *str, char rep )
#define isutf8cont( c ) (((c) >= 0x80) && ((c) <= 0xBF)) 
{
    unsigned char *ptr, c;

    ptr = (unsigned char *)str;
    while( (c = *ptr) != '\0' )
    {
        /* US-ASCII, 1 byte */
        if( c <= 0x7F )
            ptr++; /* OK */
        else
        /* 2 bytes */
        if( ( c >= 0xC2 ) && ( c <= 0xDF ) )
        {
            c = ptr[1];
            if( isutf8cont( c ) )
                ptr += 2; /* OK */
            else
                goto error;
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
                    goto error;
            }
            else
                goto error;
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
                    goto error;
            }
            else
                goto error;
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
                    goto error;
            }
            else
                goto error;
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
                        goto error;
                }
                else
                    goto error;
            }
            else
                goto error;
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
                    goto error;
                }
                else
                    goto error;
            }
            else
                goto error;
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
                        goto error;
                }
                else
                    goto error;
            }
            else
                goto error;
        }
        else
            goto error;

        continue;

error:
        if( rep == 0 )
            return NULL;
        *ptr++ = '?';
        str = NULL;
    }

    return str;
}

/**
 * EnsureUTF8: replaces invalid/overlong UTF-8 sequences with question marks
 * Note that it is not possible to convert from Latin-1 to UTF-8 on the fly,
 * so we don't try that, even though it would be less disruptive.
 *
 * @return str if it was valid UTF-8, NULL if not.
 */
char *EnsureUTF8( char *str )
{
    return CheckUTF8( str, '?' );
}


/**
 * IsUTF8: checks whether a string is a valid UTF-8 byte sequence.
 *
 * @param str nul-terminated string to be checked
 *
 * @return str if it was valid UTF-8, NULL if not.
 */
const char *IsUTF8( const char *str )
{
    return CheckUTF8( (char *)str, 0 );
}


/**
 * UTF32toUTF8(): converts an array from UTF-32 (host byte order)
 * to UTF-8.
 *
 * @param src the UTF-32 table to be converted
 * @param len the number of code points to be converted from src
 * (ie. the number of uint32_t in the table pointed to by src)
 * @param newlen an optional pointer. If not NULL, *newlen will
 * contain the total number of bytes written.
 *
 * @return the result of the conversion (must be free'd())
 * or NULL on error (in that case, *newlen is undefined).
 */
static char *
UTF32toUTF8( const uint32_t *src, size_t len, size_t *newlen )
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
        if( uv < 0x110000 )
        {
            *out++ = (( uv >> 18)         | 0xf0);
            *out++ = (((uv >> 12) & 0x3f) | 0x80);
            *out++ = (((uv >>  6) & 0x3f) | 0x80);
            *out++ = (( uv        & 0x3f) | 0x80);
            continue;
        }
        else
        {
            free( res );
            return NULL;
        }
    }
    len = out - res;
    res = realloc( res, len );
    if( newlen != NULL )
        *newlen = len;
    return res;
}

/**
 * FromUTF32(): converts an UTF-32 string to UTF-8.
 *
 * @param src UTF-32 bytes sequence, aligned on a 32-bits boundary.
 *
 * @return the result of the conversion (must be free()'d),
 * or NULL in case of error.
 */
char *FromUTF32( const uint32_t *src )
{
    const uint32_t *in;
    size_t len;

    /* determine the size of the string */
    for( len = 1, in = src; *in; len++ )
        in++;

    return UTF32toUTF8( src, len, NULL );
}

/**
 * UTF16toUTF8: converts UTF-16 (host byte order) to UTF-8
 *
 * @param src UTF-16 bytes sequence, aligned on a 16-bits boundary
 * @param len number of uint16_t to convert
 */
static char *
UTF16toUTF8( const uint16_t *in, size_t len, size_t *newlen )
{
    char *res, *out;

    /* allocate memory */
    out = res = (char *)malloc( 3 * len );
    if( res == NULL )
        return NULL;

    while( len > 0 )
    {
        uint32_t uv = *in;

        in++;
        len--;

        if( uv < 0x80 )
        {
            *out++ = uv;
            continue;
        }
        if( uv < 0x800 )
        {
            *out++ = (( uv >>  6)         | 0xc0);
            *out++ = (( uv        & 0x3f) | 0x80);
            continue;
        }
        if( (uv >= 0xd800) && (uv < 0xdbff) )
        {   /* surrogates */
            uint16_t low = GetWBE( in );
            in++;
            len--;

            if( (low < 0xdc00) || (low >= 0xdfff) )
            {
                *out++ = '?'; /* Malformed surrogate */
                continue;
            }
            else
                uv = ((uv - 0xd800) << 10) + (low - 0xdc00) + 0x10000;
        }
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


/**
 * FromUTF16(): converts an UTF-16 string to UTF-8.
 *
 * @param src UTF-16 bytes sequence, aligned on a 16-bits boundary.
 *
 * @return the result of the conversion (must be free()'d),
 * or NULL in case of error.
 */
char *FromUTF16( const uint16_t *src )
{
    const uint16_t *in;
    size_t len;

    /* determine the size of the string */
    for( len = 1, in = src; *in; len++ )
        in++;

    return UTF16toUTF8( src, len, NULL );
}
