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

#ifdef __MINGW32_VERSION
# if __MINGW32_MAJOR_VERSION == 3 && __MINGW32_MINOR_VERSION < 14
#  error This mingw-runtime is too old, it has a broken vsnprintf
# endif
/* mingw-runtime provides the whole printf family in a c99 compliant way. */
/* the way to enable this is to define __USE_MINGW_ANSI_STDIO, or something
 * such as _ISOC99_SOURCE; the former is done by configure.ac */
/* This isn't done here, since some modules don't include config.h and
 * therefore this as the first include file */
#elif defined UNDER_CE
# error Window CE support for *printf needs fixing.
#endif

#ifndef HAVE_STRDUP
char *strdup (const char *);
#endif

#ifndef HAVE_VASPRINTF
# include <stdarg.h>
int vasprintf (char **, const char *, va_list);
#endif

#ifndef HAVE_ASPRINTF
int asprintf (char **, const char *, ...);
#endif

#ifndef HAVE_STRNLEN
# include <stddef.h>
size_t strnlen (const char *, size_t);
#endif

#ifndef HAVE_STRNDUP
# include <stddef.h>
char *strndup (const char *, size_t);
#endif

#ifndef HAVE_STRLCPY
# include <stddef.h>
size_t strlcpy (char *, const char *, size_t);
#endif

#ifndef HAVE_STRTOF
float strtof (const char *, char **);
#endif

#ifndef HAVE_ATOF
double atof (const char *);
#endif

#ifndef HAVE_STRTOLL
long long int strtoll (const char *, char **, int);
#endif

#ifndef HAVE_STRSEP
char *strsep (char **, const char *);
#endif

#ifndef HAVE_ATOLL
long long atoll (const char *);
#endif

#ifndef HAVE_LLDIV
typedef struct {
    long long quot; /* Quotient. */
    long long rem;  /* Remainder. */
} lldiv_t;

lldiv_t lldiv (long long, long long);
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
