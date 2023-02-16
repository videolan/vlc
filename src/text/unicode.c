/*****************************************************************************
 * unicode.c: Unicode <-> locale functions
 *****************************************************************************
 * Copyright (C) 2005-2006 VLC authors and VideoLAN
 * Copyright © 2005-2010 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont
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
#if defined(_WIN32)
#  include <io.h>
#endif
#include <errno.h>
#include <wctype.h>

/**
 * Formats an UTF-8 string as vfprintf(), then print it, with
 * appropriate conversion to local encoding.
 */
int utf8_vfprintf( FILE *stream, const char *fmt, va_list ap )
{
#ifndef _WIN32
    return vfprintf (stream, fmt, ap);
#else
    char *str;
    int res = vasprintf (&str, fmt, ap);
    if (unlikely(res == -1))
        return -1;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
    /* Writing to the console is a lot of fun on Microsoft Windows.
     * If you use the standard I/O functions, you must use the OEM code page,
     * which is different from the usual ANSI code page. Or maybe not, if the
     * user called "chcp". Anyway, we prefer Unicode. */
    int fd = _fileno (stream);
    if (likely(fd != -1) && _isatty (fd))
    {
        wchar_t *wide = ToWide (str);
        if (likely(wide != NULL))
        {
            HANDLE h = (HANDLE)((uintptr_t)_get_osfhandle (fd));
            DWORD out;
            /* XXX: It is not clear whether WriteConsole() wants the number of
             * Unicode characters or the size of the wchar_t array. */
            BOOL ok = WriteConsoleW (h, wide, wcslen (wide), &out, NULL);
            free (wide);
            if (ok)
                goto out;
        }
    }
#endif
    wchar_t *wide = ToWide(str);
    if (likely(wide != NULL))
    {
        res = fputws(wide, stream);
        free(wide);
    }
    else
        res = -1;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
out:
#endif
    free (str);
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

ssize_t vlc_towc (const char *str, uint32_t *restrict pwc)
{
    assert (str != NULL);

    unsigned char c0 = str[0];

    if (likely((c0 & 0x80) == 0)) { // 7-bit ASCII character -> short cut
        *pwc = c0;
         return c0 != '\0';
    }

    if (unlikely((c0 & 0x40) == 0))
        return -1; // continuation byte -> error

    unsigned char c1 = str[1];
    uint32_t cp = c1 & 0x3F;

    if (unlikely((c1 >> 6) != 2)) // missing continuation byte
        return -1;

    if (likely((c0 & 0x20) == 0)) { // two-byte sequence
        *pwc = cp = ((c0 & 0x1F) << 6) | cp;

        if (unlikely(cp < 0x80))
            return -1; // ASCII overlong
        return 2;
    }

    unsigned char c2 = str[2];

    cp = (cp << 6) | (c2 & 0x3F);

    if (unlikely((c2 >> 6) != 2)) // missing second continuation byte
        return -1;

    if (likely((c0 & 0x10) == 0)) { // three-byte sequence
        *pwc = cp = ((c0 & 0xF) << 12) | cp;

        if (unlikely(cp < 0x800)) // overlong
            return -1;
        if (unlikely(cp >= 0xD800 && cp < 0xE000)) // surrogate
            return -1;
        return 3;
    }

    if (likely((c0 & 0x08) == 0)) { // four-byte sequence
        unsigned char c3 = str[3];

        cp = (cp << 6) | (c3 & 0x3F);

        if (unlikely((c3 >> 6) != 2)) // missing third continuation byte
            return -1;

        *pwc = cp = ((c0 & 0xF) << 18) | cp;

        if (unlikely(cp < 0x10000)) // overlong (or surrogate)
            return -1;
        if (unlikely(cp >= 0x110000)) // out of Unicode range
            return -1;
        return 4;
    }

    return -1;
}

/**
 * Look for an UTF-8 string within another one in a case-insensitive fashion.
 * Beware that this is quite slow. Contrary to strcasestr(), this function
 * works regardless of the system character encoding, and handles multibyte
 * code points correctly.

 * @param haystack string to look into
 * @param needle string to look for
 * @return a pointer to the first occurrence of the needle within the haystack,
 * or NULL if no occurrence were found.
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
        if (unlikely(s < 0))
            return NULL;
        haystack += s;
    }
    while (s > 0);

    return NULL;
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
