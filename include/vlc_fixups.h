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

#if !defined (HAVE_GMTIME_R) || !defined (HAVE_LOCALTIME_R)
# include <time.h> /* time_t */
#endif

#ifndef HAVE_LLDIV
typedef struct
{
    long long quot; /* Quotient. */
    long long rem;  /* Remainder. */
} lldiv_t;
#endif

#ifndef HAVE_REWIND
# include <stdio.h> /* FILE */
#endif

#if !defined (HAVE_STRLCPY) || \
    !defined (HAVE_STRNDUP) || \
    !defined (HAVE_STRNLEN) || \
    !defined (HAVE_GETCWD)
# include <stddef.h> /* size_t */
#endif

#ifndef HAVE_VASPRINTF
# include <stdarg.h> /* va_list */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_STRDUP
char *strdup (const char *);
#endif

#ifndef HAVE_VASPRINTF
int vasprintf (char **, const char *, va_list);
#endif

#ifndef HAVE_ASPRINTF
int asprintf (char **, const char *, ...);
#endif

#ifndef HAVE_STRNLEN
size_t strnlen (const char *, size_t);
#endif

#ifndef HAVE_STRNDUP
char *strndup (const char *, size_t);
#endif

#ifndef HAVE_STRLCPY
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
lldiv_t lldiv (long long, long long);
#endif

#ifndef HAVE_STRCASECMP
int strcasecmp (const char *, const char *);
#endif

#ifndef HAVE_STRNCASECMP
int strncasecmp (const char *, const char *, size_t);
#endif

#ifndef HAVE_STRCASESTR
char *strcasestr (const char *, const char *);
#endif

#ifndef HAVE_GMTIME_R
struct tm *gmtime_r (const time_t *, struct tm *);
#endif

#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r (const time_t *, struct tm *);
#endif

#ifndef HAVE_REWIND
void rewind (FILE *);
#endif

#ifndef HAVE_GETCWD
char *getcwd (char *buf, size_t size);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifndef HAVE_GETENV
static inline char *getenv (const char *name)
{
    (void)name;
    return NULL;
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
#define _(str)            vlc_gettext (str)
#define N_(str)           gettext_noop (str)
#define gettext_noop(str) (str)

#ifndef HAVE_SWAB
void swab (const void *, void *, ssize_t);
#endif

#endif /* !LIBVLC_FIXUPS_H */
