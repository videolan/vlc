/*****************************************************************************
 * fixups.h: portability fixups included from config.h
 *****************************************************************************
 * Copyright © 1998-2008 the VideoLAN project
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

/**
 * \file
 * This file is a collection of portability fixes
 */

#ifndef LIBVLC_FIXUPS_H
# define LIBVLC_FIXUPS_H 1

#ifndef HAVE_STRDUP
# include <string.h>
# include <stdlib.h>
static inline char *strdup (const char *str)
{
    size_t len = strlen (str) + 1;
    char *res = (char *)malloc (len);
    if (res) memcpy (res, str, len);
    return res;
}
#endif

#ifdef WIN32
# include <string.h>
# include <stdlib.h>
/**
 * vlc_fix_format_string:
 * @format: address of format string to fix (format string is not modified)
 *
 * Windows' printf doesn't support %z size modifiers.
 * Fix a *printf format string to make it safe for mingw/MSVCRT run times:
 *  %z* (not supported in MSVCRT) -> either %I64* or %I32.
 *
 * Returns: 1 if *format must be free()d; 0 otherwise
 */
static inline int vlc_fix_format_string (const char **format)
{
    int n = 0;
    const char *tmp = *format;
    while ((tmp = strstr (tmp, "%z")) != NULL)
    {
        n++;
        tmp += 2;
    }
    if (!n)
        return 0;

    char *dst = (char*)malloc (strlen (*format) + 2*n + 1);
    if (!dst)
    {
        *format = "vlc_fix_format_string: due to malloc failure, unable to fix unsafe string";
        return 0;
    }

    const char *src = *format;
    *format = dst;
    while ((tmp = strstr (src, "%z")) != NULL)
    {
        /* NB, don't use %l*, as this is buggy in mingw*/
        size_t d = tmp - src;
        memcpy (dst, src, d);
        dst += d;
        *dst++ = '%';
# ifdef WIN64
        *dst++ = 'I';
        *dst++ = '6';
        *dst++ = '4';
# else /* ie: WIN32 */
        /* on win32, since the default size is 32bit, dont specify
         * a modifer.  (I32 isn't on wince, l doesn't work on mingw) */
# endif
        src = tmp + 2;
    }
    strcpy (dst, src);
    return 1;
}

# include <stdio.h>
# include <stdarg.h>

static inline int vlc_vprintf (const char *format, va_list ap)
{
    int must_free = vlc_fix_format_string (&format);
    int ret = vprintf (format, ap);
    if (must_free) free ((char *)format);
    return ret;
}
# define vprintf vlc_vprintf

static inline int vlc_vfprintf (FILE *stream, const char *format, va_list ap)
{
    int must_free = vlc_fix_format_string (&format);
    int ret = vfprintf (stream, format, ap);
    if (must_free) free ((char *)format);
    return ret;
}
# define vfprintf vlc_vfprintf

static inline int vlc_vsprintf (char *str, const char *format, va_list ap)
{
    int must_free = vlc_fix_format_string (&format);
    int ret = vsprintf (str, format, ap);
    if (must_free) free ((char *)format);
    return ret;
}
# define vsprintf vlc_vsprintf

static inline int vlc_vsnprintf (char *str, size_t size, const char *format, va_list ap)
{
    int must_free = vlc_fix_format_string (&format);
    /* traditionally, MSVCRT has provided vsnprintf as _vsnprintf;
     * to 'aid' portability/standards compliance, mingw provides a
     * static version of vsnprintf that is buggy.  Be sure to use
     * MSVCRT version, at least it behaves as expected */
    int ret = _vsnprintf (str, size, format, ap);
    if (must_free) free ((char *)format);
    return ret;
}
# define vsnprintf vlc_vsnprintf

static inline int vlc_printf (const char *format, ...)
{
    va_list ap;
    int ret;
    va_start (ap, format);
    ret = vlc_vprintf (format, ap);
    va_end (ap);
    return ret;
}
# define printf(...) vlc_printf(__VA_ARGS__)

static inline int vlc_fprintf (FILE *stream, const char *format, ...)
{
    va_list ap;
    int ret;
    va_start (ap, format);
    ret = vlc_vfprintf (stream, format, ap);
    va_end (ap);
    return ret;
}
# define fprintf vlc_fprintf

#if 0
static inline int vlc_sprintf (char *str, const char *format, ...)
{
    va_list ap;
    int ret;
    va_start (ap, format);
    ret = vlc_vsprintf (str, format, ap);
    va_end (ap);
    return ret;
}
# define sprintf vlc_sprintf
#endif

static inline int vlc_snprintf (char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int ret;
    va_start (ap, format);
    ret = vlc_vsnprintf (str, size, format, ap);
    va_end (ap);
    return ret;
}
/* win32: snprintf must always be vlc_snprintf or _snprintf,
 * see comment in vlc_vsnprintf */
# define snprintf vlc_snprintf

/* Make sure we don't use flawed vasprintf or asprintf either */
# undef HAVE_VASPRINTF
# undef HAVE_ASPRINTF
#endif

#ifndef HAVE_VASPRINTF
# include <stdio.h>
# include <stdlib.h>
# include <stdarg.h>
static inline int vasprintf (char **strp, const char *fmt, va_list ap)
{
#ifndef UNDER_CE
    int len = vsnprintf (NULL, 0, fmt, ap) + 1;
    char *res = (char *)malloc (len);
    if (res == NULL)
        return -1;
    *strp = res;
    return vsnprintf (res, len, fmt, ap);
#else
    /* HACK: vsnprintf in the WinCE API behaves like
     * the one in glibc 2.0 and doesn't return the number of characters
     * it needed to copy the string.
     * cf http://msdn.microsoft.com/en-us/library/1kt27hek.aspx
     * and cf the man page of vsnprintf
     *
     Guess we need no more than 50 bytes. */
    int n, size = 50;
    char *res, *np;

    if ((res = (char *) malloc (size)) == NULL)
        return -1;

    while (1)
    {
        n = vsnprintf (res, size, fmt, ap);

        /* If that worked, return the string. */
        if (n > -1 && n < size)
        {
            *strp = res;
            return n;
        }

        /* Else try again with more space. */
        size *= 2;  /* twice the old size */

        if ((np = (char *) realloc (res, size)) == NULL)
        {
            free(res);
            return -1;
        }
        else
        {
            res = np;
        }

    }
#endif /* UNDER_CE */
}
#endif

#ifndef HAVE_ASPRINTF
# include <stdio.h>
# include <stdarg.h>
static inline int asprintf (char **strp, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start (ap, fmt);
    ret = vasprintf (strp, fmt, ap);
    va_end (ap);
    return ret;
}
#endif

#ifndef HAVE_STRNLEN
# include <string.h>
static inline size_t strnlen (const char *str, size_t max)
{
    const char *end = (const char *) memchr (str, 0, max);
    return end ? (size_t)(end - str) : max;
}
#endif

#ifndef HAVE_STRNDUP
# include <string.h>
# include <stdlib.h>
static inline char *strndup (const char *str, size_t max)
{
    size_t len = strnlen (str, max);
    char *res = (char *) malloc (len + 1);
    if (res)
    {
        memcpy (res, str, len);
        res[len] = '\0';
    }
    return res;
}
#endif

#ifndef HAVE_STRLCPY
# define strlcpy vlc_strlcpy
#endif

#ifndef HAVE_STRTOF
# define strtof( a, b ) ((float)strtod (a, b))
#endif

#ifndef HAVE_ATOF
# define atof( str ) (strtod ((str), (char **)NULL, 10))
#endif

#ifndef HAVE_STRTOLL
# define strtoll vlc_strtoll
#endif

#ifndef HAVE_STRSEP
static inline char *strsep( char **ppsz_string, const char *psz_delimiters )
{
    char *psz_string = *ppsz_string;
    if( !psz_string )
        return NULL;

    char *p = strpbrk( psz_string, psz_delimiters );
    if( !p )
    {
        *ppsz_string = NULL;
        return psz_string;
    }
    *p++ = '\0';

    *ppsz_string = p;
    return psz_string;
}
#endif

#ifndef HAVE_ATOLL
# define atoll( str ) (strtoll ((str), (char **)NULL, 10))
#endif

#ifndef HAVE_LLDIV
typedef struct {
    long long quot; /* Quotient. */
    long long rem;  /* Remainder. */
} lldiv_t;

static inline lldiv_t lldiv (long long numer, long long denom)
{
    lldiv_t d = { .quot = numer / denom, .rem = numer % denom };
    return d;
}
#endif

#ifndef HAVE_SCANDIR
# define scandir vlc_scandir
# define alphasort vlc_alphasort
#endif

#ifndef HAVE_GETENV
static inline char *getenv (const char *name)
{
    (void)name;
    return NULL;
}
#endif

#ifndef HAVE_STRCASECMP
# ifndef HAVE_STRICMP
#  include <ctype.h>
static inline int strcasecmp (const char *s1, const char *s2)
{
    for (size_t i = 0;; i++)
    {
        int d = tolower (s1[i]) - tolower (s2[i]);
        if (d || !s1[i]) return d;
    }
    return 0;
}
# else
#  define strcasecmp stricmp
# endif
#endif

#ifndef HAVE_STRNCASECMP
# ifndef HAVE_STRNICMP
#  include <ctype.h>
static inline int strncasecmp (const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        int d = tolower (s1[i]) - tolower (s2[i]);
        if (d || !s1[i]) return d;
    }
    return 0;
}
# else
#  define strncasecmp strnicmp
# endif
#endif

#ifndef HAVE_STRCASESTR
# ifndef HAVE_STRISTR
#  define strcasestr vlc_strcasestr
# else
#  define strcasestr stristr
# endif
#endif

#ifndef HAVE_LOCALTIME_R
/* If localtime_r() is not provided, we assume localtime() uses
 * thread-specific storage. */
# include <time.h>
static inline struct tm *localtime_r (const time_t *timep, struct tm *result)
{
    struct tm *s = localtime (timep);
    if (s == NULL)
        return NULL;

    *result = *s;
    return result;
}
static inline struct tm *gmtime_r (const time_t *timep, struct tm *result)
{
    struct tm *s = gmtime (timep);
    if (s == NULL)
        return NULL;

    *result = *s;
    return result;
}
#endif

/* Alignment of critical static data structures */
#ifdef ATTRIBUTE_ALIGNED_MAX
#   define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#   define ATTR_ALIGN(align)
#endif

#ifndef HAVE_USELOCALE
typedef void *locale_t;
# define newlocale( a, b, c ) ((locale_t)0)
# define uselocale( a ) ((locale_t)0)
# define freelocale( a ) (void)0
#endif

#ifdef WIN32
# include <dirent.h>
# define opendir Use_utf8_opendir_or_vlc_wopendir_instead!
# define readdir Use_utf8_readdir_or_vlc_wreaddir_instead!
# define closedir vlc_wclosedir
#endif

/* libintl support */
#define _(str) vlc_gettext (str)

#if defined (ENABLE_NLS)
# include <libintl.h>
#endif

#define N_(str) gettext_noop (str)
#define gettext_noop(str) (str)

#ifdef UNDER_CE
static inline void rewind ( FILE *stream )
{
    fseek(stream, 0L, SEEK_SET);
    clearerr(stream);
}
#endif

#endif /* !LIBVLC_FIXUPS_H */
