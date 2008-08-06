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

#ifndef HAVE_VASPRINTF
# include <stdio.h>
# include <stdlib.h>
# include <stdarg.h>
static inline int vasprintf (char **strp, const char *fmt, va_list ap)
{
    int len = vsnprintf (NULL, 0, fmt, ap) + 1;
    char *res = (char *)malloc (len);
    if (res == NULL)
        return -1;
    *strp = res;
    return vsprintf (res, fmt, ap);
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
static inline getenv (const char *name)
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

#endif /* !LIBVLC_FIXUPS_H */
