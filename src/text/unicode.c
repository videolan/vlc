/*****************************************************************************
 * unicode.c: Unicode <-> locale functions
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * Copyright © 2005-2008 Rémi Denis-Courmont
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>

#include <assert.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef UNDER_CE
#  include <tchar.h>
#endif
#include <errno.h>

#if defined (__APPLE__) || defined (HAVE_MAEMO)
/* Define this if the OS always use UTF-8 internally */
# define ASSUME_UTF8 1
#endif

#if defined (ASSUME_UTF8)
/* Cool */
#elif defined (WIN32) || defined (UNDER_CE)
# define USE_MB2MB 1
#elif defined (HAVE_ICONV)
# define USE_ICONV 1
#else
# error No UTF8 charset conversion implemented on this platform!
#endif

#if defined (USE_ICONV)
# include <langinfo.h>
static char charset[sizeof ("CSISO11SWEDISHFORNAMES")] = "";

static void find_charset_once (void)
{
    strlcpy (charset, nl_langinfo (CODESET), sizeof (charset));
    if (!strcasecmp (charset, "ASCII")
     || !strcasecmp (charset, "ANSI_X3.4-1968"))
        strcpy (charset, "UTF-8"); /* superset... */
}

static int find_charset (void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once (&once, find_charset_once);
    return !strcasecmp (charset, "UTF-8");
}
#endif


static char *locale_fast (const char *string, bool from)
{
    if( string == NULL )
        return NULL;

#if defined (USE_ICONV)
    if (find_charset ())
        return (char *)string;

    vlc_iconv_t hd = vlc_iconv_open (from ? "UTF-8" : charset,
                                     from ? charset : "UTF-8");
    if (hd == (vlc_iconv_t)(-1))
        return NULL; /* Uho! */

    const char *iptr = string;
    size_t inb = strlen (string);
    size_t outb = inb * 6 + 1;
    char output[outb], *optr = output;

    while (vlc_iconv (hd, &iptr, &inb, &optr, &outb) == (size_t)(-1))
    {
        *optr++ = '?';
        outb--;
        iptr++;
        inb--;
        vlc_iconv (hd, NULL, NULL, NULL, NULL); /* reset */
    }
    *optr = '\0';
    vlc_iconv_close (hd);

    assert (inb == 0);
    assert (*iptr == '\0');
    assert (*optr == '\0');
    assert (strlen (output) == (size_t)(optr - output));
    return strdup (output);
#elif defined (USE_MB2MB)
    char *out;
    int len;

    len = 1 + MultiByteToWideChar (from ? CP_ACP : CP_UTF8,
                                   0, string, -1, NULL, 0);
    wchar_t *wide = malloc (len * sizeof (wchar_t));
    if (wide == NULL)
        return NULL;

    MultiByteToWideChar (from ? CP_ACP : CP_UTF8, 0, string, -1, wide, len);
    len = 1 + WideCharToMultiByte (from ? CP_UTF8 : CP_ACP, 0, wide, -1,
                                   NULL, 0, NULL, NULL);
    out = malloc (len);
    if (out != NULL)
        WideCharToMultiByte (from ? CP_UTF8 : CP_ACP, 0, wide, -1, out, len,
                             NULL, NULL);
    free (wide);
    return out;
#else
    (void)from;
    return (char *)string;
#endif
}


static inline char *locale_dup (const char *string, bool from)
{
    assert( string );

#if defined (USE_ICONV)
    if (find_charset ())
        return strdup (string);
    return locale_fast (string, from);
#elif defined (USE_MB2MB)
    return locale_fast (string, from);
#else
    (void)from;
    return strdup (string);
#endif
}

/**
 * Releases (if needed) a localized or uniformized string.
 * @param str non-NULL return value from FromLocale() or ToLocale().
 */
void LocaleFree (const char *str)
{
#if defined (USE_ICONV)
    if (!find_charset ())
        free ((char *)str);
#elif defined (USE_MB2MB)
    free ((char *)str);
#else
    (void)str;
#endif
}


/**
 * Converts a string from the system locale character encoding to UTF-8.
 *
 * @param locale nul-terminated string to convert
 *
 * @return a nul-terminated UTF-8 string, or NULL in case of error.
 * To avoid memory leak, you have to pass the result to LocaleFree()
 * when it is no longer needed.
 */
char *FromLocale (const char *locale)
{
    return locale_fast (locale, true);
}

/**
 * converts a string from the system locale character encoding to utf-8,
 * the result is always allocated on the heap.
 *
 * @param locale nul-terminated string to convert
 *
 * @return a nul-terminated utf-8 string, or null in case of error.
 * The result must be freed using free() - as with the strdup() function.
 */
char *FromLocaleDup (const char *locale)
{
    return locale_dup (locale, true);
}


/**
 * ToLocale: converts an UTF-8 string to local system encoding.
 *
 * @param utf8 nul-terminated string to be converted
 *
 * @return a nul-terminated string, or NULL in case of error.
 * To avoid memory leak, you have to pass the result to LocaleFree()
 * when it is no longer needed.
 */
char *ToLocale (const char *utf8)
{
    return locale_fast (utf8, false);
}


/**
 * converts a string from UTF-8 to the system locale character encoding,
 * the result is always allocated on the heap.
 *
 * @param utf8 nul-terminated string to convert
 *
 * @return a nul-terminated string, or null in case of error.
 * The result must be freed using free() - as with the strdup() function.
 */
char *ToLocaleDup (const char *utf8)
{
    return locale_dup (utf8, false);
}

/**
 * Formats an UTF-8 string as vasprintf(), then print it to stdout, with
 * appropriate conversion to local encoding.
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

/**
 * Formats an UTF-8 string as vfprintf(), then print it, with
 * appropriate conversion to local encoding.
 */
int utf8_vfprintf( FILE *stream, const char *fmt, va_list ap )
{
    char *str;
    int res = utf8_vasprintf( &str, fmt, ap );
    if( res == -1 )
        return -1;

    fputs( str, stream );
    free( str );
    return res;
}

/**
 * Formats an UTF-8 string as fprintf(), then print it, with
 * appropriate conversion to local encoding.
 */
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
{
    uint8_t *ptr = (uint8_t *)str;
    assert (str != NULL);

    for (;;)
    {
        uint8_t c = ptr[0];

        if (c == '\0')
            break;

        if (c > 0xF4)
            goto error;

        int charlen = clz8 (c ^ 0xFF);
        switch (charlen)
        {
            case 0: // 7-bit ASCII character -> OK
                ptr++;
                continue;

            case 1: // continuation byte -> error
                goto error;
        }

        assert (charlen >= 2 && charlen <= 4);

        uint32_t cp = c & ~((0xff >> (7 - charlen)) << (7 - charlen));
        for (int i = 1; i < charlen; i++)
        {
            assert (cp < (1 << 26));
            c = ptr[i];

            if ((c >> 6) != 2) // not a continuation byte
                goto error;

            cp = (cp << 6) | (ptr[i] & 0x3f);
        }

        switch (charlen)
        {
            case 4:
                if (cp > 0x10FFFF) // beyond Unicode
                    goto error;
            case 3:
                if (cp >= 0xD800 && cp < 0xC000) // UTF-16 surrogate
                    goto error;
            case 2:
                if (cp < 128) // ASCII overlong
                    goto error;
                if (cp < (1u << (5 * charlen - 3))) // overlong
                    goto error;
        }
        ptr += charlen;
        continue;

    error:
        if (rep == 0)
            return NULL;
        *ptr++ = rep;
        str = NULL;
    }

    return str;
}

/**
 * Replaces invalid/overlong UTF-8 sequences with question marks.
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
 * Checks whether a string is a valid UTF-8 byte sequence.
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
 * Converts a string from the given character encoding to utf-8.
 *
 * @return a nul-terminated utf-8 string, or null in case of error.
 * The result must be freed using free().
 */
char *FromCharset(const char *charset, const void *data, size_t data_size)
{
    vlc_iconv_t handle = vlc_iconv_open ("UTF-8", charset);
    if (handle == (vlc_iconv_t)(-1))
        return NULL;

    char *out = NULL;
    for(unsigned mul = 4; mul < 8; mul++ )
    {
        size_t in_size = data_size;
        const char *in = data;
        size_t out_max = mul * data_size;
        char *tmp = out = malloc (1 + out_max);
        if (!out)
            break;

        if (vlc_iconv (handle, &in, &in_size, &tmp, &out_max) != (size_t)(-1)) {
            *tmp = '\0';
            break;
        }
        free(out);
        out = NULL;

        if (errno != E2BIG)
            break;
    }
    vlc_iconv_close(handle);
    return out;
}

