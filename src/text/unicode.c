/*****************************************************************************
 * unicode.c: Unicode <-> locale functions
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * Copyright © 2005-2010 Rémi Denis-Courmont
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

#include "libvlc.h"
#include <vlc_charset.h>

#include <assert.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef UNDER_CE
#  include <tchar.h>
#elif defined(WIN32)
#  include <io.h>
#endif
#include <errno.h>
#include <wctype.h>

/**
 * Releases (if needed) a localized or uniformized string.
 * @param str non-NULL return value from FromLocale() or ToLocale().
 */
void LocaleFree (const char *str)
{
#ifdef ASSUME_UTF8
    (void) str;
#else
    free ((char *)str);
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
#ifdef ASSUME_UTF8
    return (char *)locale;
#else
    return locale ? FromCharset ("", locale, strlen(locale)) : NULL;
#endif
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
    if (!locale)
        return NULL;

#ifdef ASSUME_UTF8
    return strdup (locale);
#else
    return FromCharset ("", locale, strlen(locale));
#endif
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
#ifdef ASSUME_UTF8
    return (char *)utf8;
#else
    size_t outsize;
    return utf8 ? ToCharset ("", utf8, &outsize) : NULL;
#endif
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
#ifdef ASSUME_UTF8
    return strdup (utf8);
#else
    size_t outsize;
    return ToCharset ("", utf8, &outsize);
#endif
}

/**
 * Formats an UTF-8 string as vfprintf(), then print it, with
 * appropriate conversion to local encoding.
 */
int utf8_vfprintf( FILE *stream, const char *fmt, va_list ap )
{
#ifdef ASSUME_UTF8
    return vfprintf (stream, fmt, ap);
#else
    char *str;
    int res;

# if defined( WIN32 ) && !defined( UNDER_CE )
    /* Writing to the console is a lot of fun on Microsoft Windows.
     * If you use the standard I/O functions, you must use the OEM code page,
     * which is different from the usual ANSI code page. Or maybe not, if the
     * user called "chcp". Anyway, we prefer Unicode. */
    int fd = _fileno (stream);
    if (likely(fd != -1) && _isatty (fd))
    {
        res = vasprintf (&str, fmt, ap);
        if (unlikely(res == -1))
            return -1;

        size_t wlen = 2 * (res + 1);
        wchar_t *wide = malloc (wlen);
        if (likely(wide != NULL))
        {
            wlen = MultiByteToWideChar (CP_UTF8, 0, str, res + 1, wide, wlen);
            if (wlen > 0)
            {
                HANDLE h = (HANDLE)(intptr_t)_get_osfhandle (fd);
                DWORD out;

                WriteConsoleW (h, wide, wlen - 1, &out, NULL);
            }
            else
                res = -1;
            free (wide);
        }
        else
            res = -1;
        free (str);
        return res;
    }
# endif

    res = vasprintf (&str, fmt, ap);
    if (unlikely(res == -1))
        return -1;

    char *ansi = ToLocaleDup (str);
    free (str);

    if (ansi == NULL)
        return -1;
    fputs (ansi, stream);
    free (ansi);
    return res;
#endif
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


/**
 * Converts the first character from a UTF-8 sequence into a code point.
 *
 * @param str an UTF-8 bytes sequence
 * @return 0 if str points to an empty string, i.e. the first character is NUL;
 * number of bytes that the first character occupies (from 1 to 4) otherwise;
 * -1 if the byte sequence was not a valid UTF-8 sequence.
 */
size_t vlc_towc (const char *str, uint32_t *restrict pwc)
{
    uint8_t *ptr = (uint8_t *)str, c;
    uint32_t cp;

    assert (str != NULL);

    c = *ptr;
    if (unlikely(c > 0xF4))
        return -1;

    int charlen = clz8 (c ^ 0xFF);
    switch (charlen)
    {
        case 0: // 7-bit ASCII character -> short cut
            *pwc = c;
            return c != '\0';

        case 1: // continuation byte -> error
            return -1;

        case 2:
            if (unlikely(c < 0xC2)) // ASCII overlong
                return -1;
            cp = (c & 0x1F) << 6;
            break;

        case 3:
            cp = (c & 0x0F) << 12;
            break;

        case 4:
            cp = (c & 0x07) << 16;
            break;

        default:
            assert (0);
    }

    /* Unrolled continuation bytes decoding */
    switch (charlen)
    {
        case 4:
            c = *++ptr;
            if (unlikely((c >> 6) != 2)) // not a continuation byte
                return -1;
            cp |= (c & 0x3f) << 12;

            if (unlikely(cp >= 0x110000)) // beyond Unicode range
                return -1;
            /* fall through */
        case 3:
            c = *++ptr;
            if (unlikely((c >> 6) != 2)) // not a continuation byte
                return -1;
            cp |= (c & 0x3f) << 6;

            if (unlikely(cp >= 0xD800 && cp < 0xC000)) // UTF-16 surrogate
                return -1;
            if (unlikely(cp < (1u << (5 * charlen - 4)))) // non-ASCII overlong
                return -1;
            /* fall through */
        case 2:
            c = *++ptr;
            if (unlikely((c >> 6) != 2)) // not a continuation byte
                return -1;
            cp |= (c & 0x3f);
            break;
    }

    *pwc = cp;
    return charlen;
}

/**
 * Look for an UTF-8 string within another one in a case-insensitive fashion.
 * Beware that this is quite slow. Contrary to strcasestr(), this function
 * works regardless of the system character encoding, and handles multibyte
 * code points correctly.

 * @param haystack string to look into
 * @param needle string to look for
 * @return a pointer to the first occurence of the needle within the haystack,
 * or NULL if no occurence were found.
 */
char *vlc_strcasestr (const char *haystack, const char *needle)
{
    ssize_t s;

    do
    {
        const char *h = haystack, *n = needle;

        for (;;)
        {
            uint32_t cph, cpn;

            s = vlc_towc (n, &cpn);
            if (s == 0)
                return (char *)haystack;
            if (unlikely(s < 0))
                return NULL;
            n += s;

            s = vlc_towc (h, &cph);
            if (s <= 0 || towlower (cph) != towlower (cpn))
                break;
            h += s;
        }

        s = vlc_towc (haystack, &(uint32_t) { 0 });
        haystack += s;
    }
    while (s != 0);

    return NULL;
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
    char *ret = str;
    size_t n;
    uint32_t cp;

    while ((n = vlc_towc (str, &cp)) != 0)
        if (likely(n != (size_t)-1))
            str += n;
        else
        {
            *str++ = '?';
            ret = NULL;
        }
    return ret;
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
    size_t n;
    uint32_t cp;

    while ((n = vlc_towc (str, &cp)) != 0)
        if (likely(n != (size_t)-1))
            str += n;
        else
            return NULL;
    return str;
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

/**
 * Converts a nul-terminated UTF-8 string to a given character encoding.
 * @param charset iconv name of the character set
 * @param in nul-terminated UTF-8 string
 * @param outsize pointer to hold the byte size of result
 *
 * @return A pointer to the result, which must be released using free().
 * The UTF-8 nul terminator is included in the conversion if the target
 * character encoding supports it. However it is not included in the returned
 * byte size.
 * In case of error, NULL is returned and the byte size is undefined.
 */
void *ToCharset(const char *charset, const char *in, size_t *outsize)
{
    vlc_iconv_t hd = vlc_iconv_open (charset, "UTF-8");
    if (hd == (vlc_iconv_t)(-1))
        return NULL;

    const size_t inlen = strlen (in);
    void *res;

    for (unsigned mul = 4; mul < 16; mul++)
    {
        size_t outlen = mul * (inlen + 1);
        res = malloc (outlen);
        if (unlikely(res == NULL))
            break;

        const char *inp = in;
        char *outp = res;
        size_t inb = inlen;
        size_t outb = outlen - mul;

        if (vlc_iconv (hd, &inp, &inb, &outp, &outb) != (size_t)(-1))
        {
            *outsize = outlen - mul - outb;
            outb += mul;
            inb = 1; /* append nul terminator if possible */
            if (vlc_iconv (hd, &inp, &inb, &outp, &outb) != (size_t)(-1))
                break;
            if (errno == EILSEQ) /* cannot translate nul terminator!? */
                break;
        }

        free (res);
        res = NULL;
        if (errno != E2BIG) /* conversion failure */
            break;
    }
    vlc_iconv_close (hd);
    return res;
}

