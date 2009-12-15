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

#if !defined(HAVE_GETENV) || \
    !defined(HAVE_USELOCALE)
# include <stddef.h> /* NULL */
#endif

#if !defined (HAVE_REWIND) || \
    !defined (HAVE_GETDELIM)
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

#if !defined (HAVE_GETDELIM) || \
    !defined (HAVE_GETPID)   || \
    !defined (HAVE_SWAB)
# include <sys/types.h> /* ssize_t, pid_t */
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

#ifndef HAVE_GETDELIM
ssize_t getdelim (char **, size_t *, int, FILE *);
ssize_t getline (char **, size_t *, FILE *);
#endif

#ifndef HAVE_GETPID
pid_t getpid (void);
#endif

#ifndef HAVE_STRTOK_R
char *strtok_r(char *, const char *, char **);
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
#define LC_NUMERIC_MASK  0
#define LC_MESSAGES_MASK 0
typedef void *locale_t;
static inline locale_t uselocale(locale_t loc)
{
    (void)loc;
    return NULL;
}
static inline void freelocale(locale_t loc)
{
    (void)loc;
}
static inline locale_t newlocale(int mask, const char * locale, locale_t base)
{
    (void)mask; (void)locale; (void)base;
    return NULL;
}
#endif

#ifdef WIN32
# include <dirent.h>
# define opendir Use_vlc_opendir_or_vlc_wopendir_instead!
# define readdir Use_vlc_readdir_or_vlc_wreaddir_instead!
# define closedir vlc_wclosedir
#endif

/* libintl support */
#define _(str)            vlc_gettext (str)
#define N_(str)           gettext_noop (str)
#define gettext_noop(str) (str)

#ifndef HAVE_SWAB
void swab (const void *, void *, ssize_t);
#endif

/* Socket stuff */
#ifndef HAVE_INET_PTON
# define inet_pton vlc_inet_pton
#endif

#ifndef HAVE_INET_NTOP
# define inet_ntop vlc_inet_ntop
#endif

#ifndef HAVE_POLL
enum
{
    POLLIN=1,
    POLLOUT=2,
    POLLPRI=4,
    POLLERR=8,  // unsupported stub
    POLLHUP=16, // unsupported stub
    POLLNVAL=32 // unsupported stub
};

struct pollfd
{
    int fd;
    unsigned events;
    unsigned revents;
};

# define poll(a, b, c) vlc_poll(a, b, c)
#elif defined (HAVE_MAEMO)
# include <poll.h>
# define poll(a, b, c) vlc_poll(a, b, c)
int vlc_poll (struct pollfd *, unsigned, int);
#endif

#ifndef HAVE_TDESTROY
# define tdestroy vlc_tdestroy
#endif

/* Random numbers */
#ifndef HAVE_NRAND48
double erand48 (unsigned short subi[3]);
long jrand48 (unsigned short subi[3]);
long nrand48 (unsigned short subi[3]);
#endif

#endif /* !LIBVLC_FIXUPS_H */
