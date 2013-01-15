/*****************************************************************************
 * vlc_fixups.h: portability fixups included from config.h
 *****************************************************************************
 * Copyright © 1998-2008 the VideoLAN project
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

#if !defined (HAVE_POSIX_MEMALIGN) || \
    !defined (HAVE_STRLCPY) || \
    !defined (HAVE_STRNDUP) || \
    !defined (HAVE_STRNLEN)
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

#if !defined (HAVE_DIRFD) || \
    !defined (HAVE_FDOPENDIR)
# include <dirent.h>
#endif

#ifdef __cplusplus
# define VLC_NOTHROW throw ()
extern "C" {
#else
# define VLC_NOTHROW
#endif

/* stdio.h */
#ifndef HAVE_ASPRINTF
int asprintf (char **, const char *, ...);
#endif

#ifndef HAVE_FLOCKFILE
void flockfile (FILE *);
int ftrylockfile (FILE *);
void funlockfile (FILE *);
int getc_unlocked (FILE *);
int getchar_unlocked (void);
int putc_unlocked (int, FILE *);
int putchar_unlocked (int);
#endif

#ifndef HAVE_GETDELIM
ssize_t getdelim (char **, size_t *, int, FILE *);
ssize_t getline (char **, size_t *, FILE *);
#endif

#ifndef HAVE_REWIND
void rewind (FILE *);
#endif

#ifndef HAVE_VASPRINTF
int vasprintf (char **, const char *, va_list);
#endif

/* string.h */
#ifndef HAVE_STRCASECMP
int strcasecmp (const char *, const char *);
#endif

#ifndef HAVE_STRCASESTR
char *strcasestr (const char *, const char *);
#endif

#ifndef HAVE_STRDUP
char *strdup (const char *);
#endif

#ifndef HAVE_STRVERSCMP
int strverscmp (const char *, const char *);
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

#ifndef HAVE_STRSEP
char *strsep (char **, const char *);
#endif

#ifndef HAVE_STRTOK_R
char *strtok_r(char *, const char *, char **);
#endif

/* stdlib.h */
#ifndef HAVE_ATOF
#ifndef __ANDROID__
double atof (const char *);
#endif
#endif

#ifndef HAVE_ATOLL
long long atoll (const char *);
#endif

#ifndef HAVE_LLDIV
lldiv_t lldiv (long long, long long);
#endif

#ifndef HAVE_STRTOF
#ifndef __ANDROID__
float strtof (const char *, char **);
#endif
#endif

#ifndef HAVE_STRTOLL
long long int strtoll (const char *, char **, int);
#endif

/* time.h */
#ifndef HAVE_GMTIME_R
struct tm *gmtime_r (const time_t *, struct tm *);
#endif

#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r (const time_t *, struct tm *);
#endif

/* unistd.h */
#ifndef HAVE_GETPID
pid_t getpid (void) VLC_NOTHROW;
#endif

#ifndef HAVE_FSYNC
int fsync (int fd);
#endif

/* dirent.h */
#ifndef HAVE_DIRFD
int (dirfd) (DIR *);
#endif

#ifndef HAVE_FDOPENDIR
DIR *fdopendir (int);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

/* stdlib.h */
#ifndef HAVE_GETENV
static inline char *getenv (const char *name)
{
    (void)name;
    return NULL;
}
#endif

#ifndef HAVE_SETENV
int setenv (const char *, const char *, int);
int unsetenv (const char *);
#endif

#ifndef HAVE_POSIX_MEMALIGN
int posix_memalign (void **, size_t, size_t);
#endif

/* locale.h */
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

#if !defined (HAVE_STATIC_ASSERT)
# define _Static_assert(x, s) ((void) sizeof (struct { unsigned:-!(x); }))
# define static_assert _Static_assert
#endif

/* Alignment of critical static data structures */
#ifdef ATTRIBUTE_ALIGNED_MAX
#   define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#   define ATTR_ALIGN(align)
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
int inet_pton(int, const char *, void *);
const char *inet_ntop(int, const void *, char *, int);
#endif

#ifndef HAVE_STRUCT_POLLFD
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
#endif
#ifndef HAVE_POLL
struct pollfd;
int poll (struct pollfd *, unsigned, int);
#endif

#ifndef HAVE_IF_NAMEINDEX
#include <errno.h>
struct if_nameindex
{
    unsigned if_index;
    char    *if_name;
};
# ifndef HAVE_IF_NAMETOINDEX
#  define if_nametoindex(name)   atoi(name)
# endif
# define if_nameindex()         (errno = ENOBUFS, NULL)
# define if_freenameindex(list) (void)0
#endif

/* search.h */
#ifndef HAVE_SEARCH_H
typedef struct entry {
    char *key;
    void *data;
} ENTRY;

typedef enum {
    FIND, ENTER
} ACTION;

typedef enum {
    preorder,
    postorder,
    endorder,
    leaf
} VISIT;

void *tsearch( const void *key, void **rootp, int(*cmp)(const void *, const void *) );
void *tfind( const void *key, const void **rootp, int(*cmp)(const void *, const void *) );
void *tdelete( const void *key, void **rootp, int(*cmp)(const void *, const void *) );
void twalk( const void *root, void(*action)(const void *nodep, VISIT which, int depth) );
void tdestroy( void *root, void (*free_node)(void *nodep) );
#else // HAVE_SEARCH_H
# ifndef HAVE_TDESTROY
#  define tdestroy vlc_tdestroy
# endif
#endif

/* Random numbers */
#ifndef HAVE_NRAND48
double erand48 (unsigned short subi[3]);
long jrand48 (unsigned short subi[3]);
long nrand48 (unsigned short subi[3]);
#endif

#ifdef __OS2__
# undef HAVE_FORK   /* Implementation of fork() is imperfect on OS/2 */
#endif

#endif /* !LIBVLC_FIXUPS_H */
