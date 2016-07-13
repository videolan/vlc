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

/* needed to detect uClibc */
#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

/* C++11 says there's no need to define __STDC_*_MACROS when including
 * inttypes.h and stdint.h. */
#if defined (__cplusplus) && (!defined(HAVE_CXX11) || defined(__MINGW32__) || defined(__UCLIBC__))
# ifndef __STDC_FORMAT_MACROS
#  define __STDC_FORMAT_MACROS 1
# endif
# ifndef __STDC_CONSTANT_MACROS
#  define __STDC_CONSTANT_MACROS 1
# endif
# ifndef __STDC_LIMIT_MACROS
#  define __STDC_LIMIT_MACROS 1
# endif
#endif

#if !defined (HAVE_GMTIME_R) || !defined (HAVE_LOCALTIME_R) \
 || !defined (HAVE_TIMEGM)
# include <time.h> /* time_t */
#endif

#ifndef HAVE_GETTIMEOFDAY
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif
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
    !defined (HAVE_MEMRCHR) || \
    !defined (HAVE_STRLCPY) || \
    !defined (HAVE_STRNDUP) || \
    !defined (HAVE_STRNLEN) || \
    !defined (HAVE_STRNSTR)
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
#ifndef HAVE_FFSLL
int ffsll(unsigned long long);
#endif

#ifndef HAVE_MEMRCHR
void *memrchr(const void *, int, size_t);
#endif

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

#ifndef HAVE_STRNSTR
char * strnstr (const char *, const char *, size_t);
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

#ifndef HAVE_TIMEGM
time_t timegm(struct tm *);
#endif

#ifndef HAVE_TIMESPEC_GET
#define TIME_UTC 1
struct timespec;
int timespec_get(struct timespec *, int);
#endif

/* sys/time.h */
#ifndef HAVE_GETTIMEOFDAY
struct timezone;
int gettimeofday(struct timeval *, struct timezone *);
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
#define LC_ALL_MASK      0
#define LC_NUMERIC_MASK  0
#define LC_MESSAGES_MASK 0
#define LC_GLOBAL_LOCALE ((locale_t)(uintptr_t)1)
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

#if !defined (HAVE_STATIC_ASSERT) && !defined(__cpp_static_assert)
# define STATIC_ASSERT_CONCAT_(a, b) a##b
# define STATIC_ASSERT_CONCAT(a, b) STATIC_ASSERT_CONCAT_(a, b)
# define _Static_assert(x, s) extern char STATIC_ASSERT_CONCAT(static_assert_, __LINE__)[sizeof(struct { unsigned:-!(x); })]
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_SWAB
void swab (const void *, void *, ssize_t);
#endif

/* Socket stuff */
#ifndef HAVE_INET_PTON
# ifndef _WIN32
#  include <sys/socket.h>
#else
typedef int socklen_t;
# endif
int inet_pton(int, const char *, void *);
const char *inet_ntop(int, const void *, char *, socklen_t);
#endif

#ifndef HAVE_STRUCT_POLLFD
enum
{
    POLLERR=0x1,
    POLLHUP=0x2,
    POLLNVAL=0x4,
    POLLWRNORM=0x10,
    POLLWRBAND=0x20,
    POLLRDNORM=0x100,
    POLLRDBAND=0x200,
    POLLPRI=0x400,
};
#define POLLIN  (POLLRDNORM|POLLRDBAND)
#define POLLOUT (POLLWRNORM|POLLWRBAND)

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

#ifndef HAVE_STRUCT_TIMESPEC
struct timespec {
    time_t  tv_sec;   /* Seconds */
    long    tv_nsec;  /* Nanoseconds */
};
#endif

#ifdef _WIN32
struct iovec
{
    void  *iov_base;
    size_t iov_len;
};
#define IOV_MAX 255
struct msghdr
{
    void         *msg_name;
    size_t        msg_namelen;
    struct iovec *msg_iov;
    size_t        msg_iovlen;
    void         *msg_control;
    size_t        msg_controllen;
    int           msg_flags;
};
#endif

#ifndef HAVE_RECVMSG
struct msghdr;
ssize_t recvmsg(int, struct msghdr *, int);
#endif

#ifndef HAVE_SENDMSG
struct msghdr;
ssize_t sendmsg(int, const struct msghdr *, int);
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
void vlc_tdestroy( void *, void (*)(void *) );
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

struct addrinfo
{
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

void freeaddrinfo (struct addrinfo *res);
#endif

/* math.h */

#ifndef HAVE_NANF
#define nanf(tagp) NAN
#endif

#ifdef _WIN32
FILE *vlc_win32_tmpfile(void);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !LIBVLC_FIXUPS_H */
