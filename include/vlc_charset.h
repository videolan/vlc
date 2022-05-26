/*****************************************************************************
 * vlc_charset.h: Unicode UTF-8 wrappers function
 *****************************************************************************
 * Copyright (C) 2003-2005 VLC authors and VideoLAN
 * Copyright © 2005-2010 Rémi Denis-Courmont
 *
 * Author: Rémi Denis-Courmont
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

#ifndef VLC_CHARSET_H
#define VLC_CHARSET_H 1

/**
 * \file vlc_charset.h
 * \ingroup charset
 * \defgroup charset Character sets
 * \ingroup strings
 * @{
 */

/**
 * Decodes a code point from UTF-8.
 *
 * Converts the first character in a UTF-8 sequence into a Unicode code point.
 *
 * \param str an UTF-8 bytes sequence [IN]
 * \param pwc address of a location to store the code point [OUT]
 *
 * \return the number of bytes occupied by the decoded code point
 *
 * \retval (size_t)-1 not a valid UTF-8 sequence
 * \retval 0 null character (i.e. str points to an empty string)
 * \retval 1 (non-null) ASCII character
 * \retval 2-4 non-ASCII character
 */
VLC_API size_t vlc_towc(const char *str, uint32_t *restrict pwc);

/**
 * Checks UTF-8 validity.
 *
 * Checks whether a null-terminated string is a valid UTF-8 bytes sequence.
 *
 * \param str string to check
 *
 * \retval str the string is a valid null-terminated UTF-8 sequence
 * \retval NULL the string is not an UTF-8 sequence
 */
VLC_USED static inline const char *IsUTF8(const char *str)
{
    size_t n;
    uint32_t cp;

    while ((n = vlc_towc(str, &cp)) != 0)
        if (likely(n != (size_t)-1))
            str += n;
        else
            return NULL;
    return str;
}

/**
 * Checks ASCII validity.
 *
 * Checks whether a null-terminated string is a valid ASCII bytes sequence
 * (non-printable ASCII characters 1-31 are permitted).
 *
 * \param str string to check
 *
 * \retval str the string is a valid null-terminated ASCII sequence
 * \retval NULL the string is not an ASCII sequence
 */
VLC_USED static inline const char *IsASCII(const char *str)
{
    unsigned char c;

    for (const char *p = str; (c = *p) != '\0'; p++)
        if (c >= 0x80)
            return NULL;
    return str;
}

/**
 * Removes non-UTF-8 sequences.
 *
 * Replaces invalid or <i>over-long</i> UTF-8 bytes sequences within a
 * null-terminated string with question marks. This is so that the string can
 * be printed at least partially.
 *
 * \warning Do not use this were correctness is critical. use IsUTF8() and
 * handle the error case instead. This function is mainly for display or debug.
 *
 * \note Converting from Latin-1 to UTF-8 in place is not possible (the string
 * size would be increased). So it is not attempted even if it would otherwise
 * be less disruptive.
 *
 * \retval str the string is a valid null-terminated UTF-8 sequence
 *             (i.e. no changes were made)
 * \retval NULL the string is not an UTF-8 sequence
 */
static inline char *EnsureUTF8(char *str)
{
    char *ret = str;
    size_t n;
    uint32_t cp;

    while ((n = vlc_towc(str, &cp)) != 0)
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
 * \defgroup iconv iconv wrappers
 *
 * (defined in src/extras/libc.c)
 * @{
 */

#define VLC_ICONV_ERR ((size_t) -1)
typedef void *vlc_iconv_t;
VLC_API vlc_iconv_t vlc_iconv_open( const char *, const char * ) VLC_USED;
VLC_API size_t vlc_iconv( vlc_iconv_t, const char **, size_t *, char **, size_t * ) VLC_USED;
VLC_API int vlc_iconv_close( vlc_iconv_t );

/** @} */

#include <stdarg.h>

VLC_API int utf8_vfprintf( FILE *stream, const char *fmt, va_list ap );
VLC_API int utf8_fprintf( FILE *, const char *, ... ) VLC_FORMAT( 2, 3 );
VLC_API char * vlc_strcasestr(const char *, const char *) VLC_USED;

VLC_API char * FromCharset( const char *charset, const void *data, size_t data_size ) VLC_USED;
VLC_API void * ToCharset( const char *charset, const char *in, size_t *outsize ) VLC_USED;

#ifdef __APPLE__
# include <CoreFoundation/CoreFoundation.h>

/* Obtains a copy of the contents of a CFString in specified encoding.
 * Returns char* (must be freed by caller) or NULL on failure.
 */
VLC_USED static inline char *FromCFString(const CFStringRef cfString,
    const CFStringEncoding cfStringEncoding)
{
    // Try the quick way to obtain the buffer
    const char *tmpBuffer = CFStringGetCStringPtr(cfString, cfStringEncoding);

    if (tmpBuffer != NULL) {
       return strdup(tmpBuffer);
    }

    // The quick way did not work, try the long way
    CFIndex length = CFStringGetLength(cfString);
    CFIndex maxSize =
        CFStringGetMaximumSizeForEncoding(length, cfStringEncoding);

    // If result would exceed LONG_MAX, kCFNotFound is returned
    if (unlikely(maxSize == kCFNotFound)) {
        return NULL;
    }

    // Account for the null terminator
    maxSize++;

    char *buffer = (char *)malloc(maxSize);

    if (unlikely(buffer == NULL)) {
        return NULL;
    }

    // Copy CFString in requested encoding to buffer
    Boolean success = CFStringGetCString(cfString, buffer, maxSize, cfStringEncoding);

    if (!success)
        FREENULL(buffer);
    return buffer;
}
#endif

#ifdef _WIN32
VLC_USED
static inline char *FromWide (const wchar_t *wide)
{
    size_t len = WideCharToMultiByte (CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len == 0)
        return NULL;

    char *out = (char *)malloc (len);

    if (likely(out))
        WideCharToMultiByte (CP_UTF8, 0, wide, -1, out, len, NULL, NULL);
    return out;
}

VLC_USED
static inline wchar_t *ToWide (const char *utf8)
{
    int len = MultiByteToWideChar (CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len == 0)
        return NULL;

    wchar_t *out = (wchar_t *)malloc (len * sizeof (wchar_t));

    if (likely(out))
        MultiByteToWideChar (CP_UTF8, 0, utf8, -1, out, len);
    return out;
}

VLC_USED VLC_MALLOC
static inline char *ToCodePage (unsigned cp, const char *utf8)
{
    wchar_t *wide = ToWide (utf8);
    if (wide == NULL)
        return NULL;

    size_t len = WideCharToMultiByte (cp, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len == 0) {
        free(wide);
        return NULL;
    }

    char *out = (char *)malloc (len);
    if (likely(out != NULL))
        WideCharToMultiByte (cp, 0, wide, -1, out, len, NULL, NULL);
    free (wide);
    return out;
}

VLC_USED VLC_MALLOC
static inline char *FromCodePage (unsigned cp, const char *mb)
{
    int len = MultiByteToWideChar (cp, 0, mb, -1, NULL, 0);
    if (len == 0)
        return NULL;

    wchar_t *wide = (wchar_t *)malloc (len * sizeof (wchar_t));
    if (unlikely(wide == NULL))
        return NULL;
    MultiByteToWideChar (cp, 0, mb, -1, wide, len);

    char *utf8 = FromWide (wide);
    free (wide);
    return utf8;
}

VLC_USED VLC_MALLOC
static inline char *FromANSI (const char *ansi)
{
    return FromCodePage (GetACP (), ansi);
}

VLC_USED VLC_MALLOC
static inline char *ToANSI (const char *utf8)
{
    return ToCodePage (GetACP (), utf8);
}

# define FromLocale    FromANSI
# define ToLocale      ToANSI
# define LocaleFree(s) free((char *)(s))
# define FromLocaleDup FromANSI
# define ToLocaleDup   ToANSI

#elif defined(__OS2__)

VLC_USED static inline char *FromLocale (const char *locale)
{
    return locale ? FromCharset ((char *)"", locale, strlen(locale)) : NULL;
}

VLC_USED static inline char *ToLocale (const char *utf8)
{
    size_t outsize;
    return utf8 ? (char *)ToCharset ("", utf8, &outsize) : NULL;
}

VLC_USED static inline void LocaleFree (const char *str)
{
    free ((char *)str);
}

VLC_USED static inline char *FromLocaleDup (const char *locale)
{
    return FromCharset ("", locale, strlen(locale));
}

VLC_USED static inline char *ToLocaleDup (const char *utf8)
{
    size_t outsize;
    return (char *)ToCharset ("", utf8, &outsize);
}

#else

# define FromLocale(l) (l)
# define ToLocale(u)   (u)
# define LocaleFree(s) ((void)(s))
# define FromLocaleDup strdup
# define ToLocaleDup   strdup
#endif

/**
 * Converts a nul-terminated string from ISO-8859-1 to UTF-8.
 */
static inline char *FromLatin1 (const char *latin)
{
    char *str = (char *)malloc (2 * strlen (latin) + 1), *utf8 = str;
    unsigned char c;

    if (str == NULL)
        return NULL;

    while ((c = *(latin++)) != '\0')
    {
         if (c >= 0x80)
         {
             *(utf8++) = 0xC0 | (c >> 6);
             *(utf8++) = 0x80 | (c & 0x3F);
         }
         else
             *(utf8++) = c;
    }
    *(utf8++) = '\0';

    utf8 = (char *)realloc (str, utf8 - str);
    return utf8 ? utf8 : str;
}

/**
 * \defgroup c_locale C/POSIX locale functions
 * @{
 */

/**
 * Parses a double in C locale.
 *
 * This function parses a double-precision floating point number from a string
 * just like the standard strtod() but it uses the C locale. In other words, it
 * expects the POSIX/C/American decimal format regardless of the current
 * numeric locale.
 *
 * \param str nul-terminated string to parse
 * \param[out] end storage space for a pointer to the first unparsed byte
 *                 (or NULL to discard it)
 * \return the parsed double value (zero if no character could be parsed)
 */
VLC_API double vlc_strtod_c(const char *restrict str, char **restrict end)
VLC_USED;

/**
 * Parses a float in C locale.
 *
 * This function parses a single-precision floating point number from a string
 * just like the standard strtof() but it uses the C locale. In other words, it
 * expects the POSIX/C/American decimal format regardless of the current
 * numeric locale.
 *
 * \param str nul-terminated string to parse
 * \param[out] end storage space for a pointer to the first unparsed byte
 *                 (or NULL to discard it)
 * \return the parsed double value (zero if no character could be parsed)
 */
VLC_API float vlc_strtof_c(const char *restrict str, char **restrict end)
VLC_USED;

/**
 * Parses a double in C locale.
 *
 * This function parses a double-precision floating point number from a string
 * just like the standard atof() but it uses the C locale. In other words, it
 * expects the POSIX/C/American decimal format regardless of the current
 * numeric locale.
 *
 * \param str nul-terminated string to parse
 * \return the parsed double value (zero if no character could be parsed)
 */
VLC_USED static inline double vlc_atof_c(const char *str)
{
    return vlc_strtod_c(str, NULL);
}

/**
 * Formats a string using the C locale.
 *
 * This function formats a string from a format string and a variable argument
 * list, just like the standard vasprintf() but using the C locale for the
 * formatting of numerals.
 *
 * \param[out] p storage space for a pointer to the heap-allocated formatted
 *               string (undefined on error)
 * \param fmt format string
 * \param ap variable argument list
 * \return number of bytes formatted (excluding the nul terminator)
 *        or -1 on error
 */
VLC_API int vlc_vasprintf_c(char **restrict p, const char *restrict fmt,
                            va_list ap) VLC_USED;

/**
 * Formats a string using the C locale.
 *
 * This function formats a string from a format string and a variable argument
 * list, just like the standard vasprintf() but using the C locale for the
 * formatting of numerals.
 *
 * \param[out] p storage space for a pointer to the heap-allocated formatted
 *               string (undefined on error)
 * \param fmt format string
 * \return number of bytes formatted (excluding the nul terminator)
 *        or -1 on error
 */
VLC_API int vlc_asprintf_c( char **, const char *, ... ) VLC_USED;

int vlc_vsscanf_c(const char *, const char *, va_list) VLC_USED;
int vlc_sscanf_c(const char*, const char*, ...) VLC_USED
#ifdef __GNUC__
__attribute__((format(scanf, 2, 3)))
#endif
;

/** @} */
/** @} */

#endif
