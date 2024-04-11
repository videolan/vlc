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

#if defined(_MSC_VER)
// disable common warnings when compiling POSIX code
#ifndef _CRT_NONSTDC_NO_DEPRECATE
// the POSIX variants are not available in the GDK
# if !(defined(_GAMING_XBOX_SCARLETT) || defined(_GAMING_XBOX_XBOXONE) || defined(_XBOX_ONE))
#  define _CRT_NONSTDC_NO_DEPRECATE
# endif
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS     1
#endif
#if defined(_GAMING_XBOX_SCARLETT) || defined(_GAMING_XBOX_XBOXONE) || defined(_XBOX_ONE)
// make sure we don't use MS POSIX aliases that won't link
# undef _CRT_DECLARE_NONSTDC_NAMES
# define _CRT_DECLARE_NONSTDC_NAMES 0
#endif


// sys/stat.h values
#define S_IWUSR     _S_IWRITE
#define S_IRUSR     _S_IREAD
#define S_IFIFO     _S_IFIFO
#define S_IFMT      _S_IFMT
#define S_IFCHR     _S_IFCHR
#define S_IFREG     _S_IFREG
#define S_IFDIR     _S_IFDIR
#define S_ISDIR(m)  (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m)  (((m) & _S_IFMT) == _S_IFREG)
#define S_ISBLK(m)  (0)

// same type as statXXX structures st_mode field
typedef unsigned short mode_t;

// no compat, but there's an MSVC equivalent
#define strncasecmp _strnicmp
#define snwprintf   _snwprintf

// since we define restrist as __restrict for C++, __declspec(restrict) is bogus
#define _CRT_SUPPRESS_RESTRICT
#define DECLSPEC_RESTRICT

// turn CPU MSVC-ism into more standard defines
#if defined(_M_X64) && !defined(__x86_64__)
# define __x86_64__
#endif
#if defined(_M_IX86) && !defined(__i386__)
# define __i386__
#endif
#if defined(_M_ARM64) && !defined(__aarch64__)
# define __aarch64__
#endif
#if defined(_M_ARM) && !defined(__arm__)
# define __arm__
#endif
#if defined(_M_IX86_FP) && _M_IX86_FP == 1 && !defined(__SSE__)
# define __SSE__
#endif
#if defined(_M_IX86_FP) && _M_IX86_FP == 2 && !defined(__SSE2__)
# define __SSE2__
#endif

#endif // _MSC_VER

#ifdef _WIN32
# if !defined(NOMINMAX)
// avoid collision between numeric_limits::max() and max define
#  define NOMINMAX
# endif
# if !defined(_USE_MATH_DEFINES)
// enable M_PI definition
#  define _USE_MATH_DEFINES
# endif
#endif


/* needed to detect uClibc */
#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

/* C++11 says there's no need to define __STDC_*_MACROS when including
 * inttypes.h and stdint.h. */
#if defined (__cplusplus) && defined(__UCLIBC__)
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

#ifndef __cplusplus
# ifdef HAVE_THREADS_H
#  include <threads.h>
# elif !defined(thread_local)
#  ifdef HAVE_THREAD_LOCAL
#   define thread_local _Thread_local
#  elif defined(_MSC_VER)
#   define thread_local __declspec(thread)
#  endif
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

#if !defined (HAVE_ALIGNED_ALLOC) || \
    !defined (HAVE_MEMRCHR) || \
    !defined (HAVE_POSIX_MEMALIGN) || \
    !defined (HAVE_QSORT_R) || \
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
    !defined (HAVE_SWAB) || \
    !defined (HAVE_WRITEV) || \
    !defined (HAVE_READV)
# include <sys/types.h> /* ssize_t, pid_t */

# if defined(_CRT_INTERNAL_NONSTDC_NAMES) && !_CRT_INTERNAL_NONSTDC_NAMES
// MS POSIX aliases missing
typedef _off_t off_t;
# endif
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

/* stddef.h */
#if !defined (__cplusplus) && !defined (HAVE_MAX_ALIGN_T)
typedef struct {
  long long ll;
  long double ld;
} max_align_t;
#endif

/* stdio.h */
#ifndef HAVE_ASPRINTF
int asprintf (char **, const char *, ...);
#endif

#ifndef HAVE_FLOCKFILE
void flockfile (FILE *);
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

/* sys/uio.h */
#ifndef HAVE_READV
struct iovec;
ssize_t readv(int, const struct iovec *, int);
#endif

#ifndef HAVE_WRITEV
struct iovec;
ssize_t writev(int, const struct iovec *, int);
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
#ifndef TIME_UTC
#define TIME_UTC 1
#endif
struct timespec;
int timespec_get(struct timespec *, int);
#endif

/* sys/time.h */
#ifndef HAVE_GETTIMEOFDAY
struct timezone;
int gettimeofday(struct timeval *, struct timezone *);
#endif

#if defined(WIN32) && !defined(WINSTORECOMPAT)
#include <winapifamily.h>
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
// getpid is incorrectly detected in UWP so we won't use the compat version
#include <processthreadsapi.h>
#define getpid()  GetCurrentProcessId()
#endif
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_SETENV
int setenv (const char *, const char *, int);
int unsetenv (const char *);
#endif

#ifndef HAVE_POSIX_MEMALIGN
int posix_memalign(void **, size_t, size_t);
#endif

#ifndef HAVE_ALIGNED_ALLOC
void *aligned_alloc(size_t, size_t);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#if defined (_WIN32)
#define aligned_free(ptr)  _aligned_free(ptr)
#else
#define aligned_free(ptr)  free(ptr)
#endif

#if !defined(HAVE_NEWLOCALE) && defined(HAVE_CXX_LOCALE_T) && defined(__cplusplus)
# include <locale>
# define HAVE_NEWLOCALE
#endif

/* locale.h */
#ifndef HAVE_USELOCALE
# ifndef HAVE_NEWLOCALE
#  define LC_ALL_MASK      0
#  define LC_NUMERIC_MASK  0
#  define LC_MESSAGES_MASK 0
#  define LC_GLOBAL_LOCALE ((locale_t)(uintptr_t)1)
typedef void *locale_t;

static inline void freelocale(locale_t loc)
{
    (void)loc;
}
static inline locale_t newlocale(int mask, const char * locale, locale_t base)
{
    (void)mask; (void)locale; (void)base;
    return NULL;
}
# else
#  include <locale.h>
# endif

static inline locale_t uselocale(locale_t loc)
{
    (void)loc;
    return NULL;
}
#endif

/* libintl support */
#define _(str)            vlc_gettext (str)
#define N_(str)           gettext_noop (str)
#define gettext_noop(str) (str)

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_SWAB
/* Android NDK25 have swab but configure fails to detect it */
#ifndef __ANDROID__
void swab (const void *, void *, ssize_t);
#endif
#endif

/* Socket stuff */
#ifndef HAVE_INET_PTON
# ifdef __cplusplus
}
# endif
# ifndef _WIN32
#  include <sys/socket.h>
#else
typedef int socklen_t;
# endif
# ifdef __cplusplus
extern "C" {
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
    short events;
    short revents;
};
#endif
#ifndef HAVE_POLL
struct pollfd;
int poll (struct pollfd *, unsigned, int);
#endif

#ifndef HAVE_IF_NAMEINDEX
# ifdef __cplusplus
}
# endif
#include <errno.h>
# ifdef __cplusplus
extern "C" {
# endif
# ifndef HAVE_STRUCT_IF_NAMEINDEX
struct if_nameindex
{
    unsigned if_index;
    char    *if_name;
};
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

# ifndef HAVE_IF_NAMETOINDEX
#  ifdef __cplusplus
}
#  endif
#  include <stdlib.h> /* a define may change from the real atoi declaration */
#  ifdef __cplusplus
extern "C" {
#  endif
static inline int if_nametoindex(const char *name)
{
    return atoi(name);
}
# endif
#endif

#ifdef _NEWLIB_VERSION
#define IOV_MAX 255
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
#ifndef HAVE_TFIND
typedef enum {
    preorder,
    postorder,
    endorder,
    leaf
} VISIT;

void *tsearch( const void *key, void **rootp, int(*cmp)(const void *, const void *) );
void *tfind( const void *key, void * const *rootp, int(*cmp)(const void *, const void *) );
void *tdelete( const void *key, void **rootp, int(*cmp)(const void *, const void *) );
void twalk( const void *root, void(*action)(const void *nodep, VISIT which, int depth) );
#ifndef _WIN32
/* the Win32 prototype of lfind() expects an unsigned* for 'nmemb' */
void *lfind( const void *key, const void *base, size_t *nmemb,
             size_t size, int(*cmp)(const void *, const void *) );
#endif
#endif /* HAVE_TFIND */

#ifndef HAVE_TDESTROY
void tdestroy( void *root, void (*free_node)(void *nodep) );
#endif

/* sys/auxv.h */
#ifndef HAVE_GETAUXVAL
unsigned long getauxval(unsigned long);
#endif

/* Random numbers */
#ifndef HAVE_NRAND48
double erand48 (unsigned short subi[3]);
long jrand48 (unsigned short subi[3]);
long nrand48 (unsigned short subi[3]);
#endif

#ifdef __OS2__
# undef HAVE_FORK   /* Implementation of fork() is imperfect on OS/2 */

# define SHUT_RD    0
# define SHUT_WR    1
# define SHUT_RDWR  2

/* GAI error codes */
# ifndef EAI_BADFLAGS
#  define EAI_BADFLAGS -1
# endif
# ifndef EAI_NONAME
#  define EAI_NONAME -2
# endif
# ifndef EAI_AGAIN
#  define EAI_AGAIN -3
# endif
# ifndef EAI_FAIL
#  define EAI_FAIL -4
# endif
# ifndef EAI_NODATA
#  define EAI_NODATA -5
# endif
# ifndef EAI_FAMILY
#  define EAI_FAMILY -6
# endif
# ifndef EAI_SOCKTYPE
#  define EAI_SOCKTYPE -7
# endif
# ifndef EAI_SERVICE
#  define EAI_SERVICE -8
# endif
# ifndef EAI_ADDRFAMILY
#  define EAI_ADDRFAMILY -9
# endif
# ifndef EAI_MEMORY
#  define EAI_MEMORY -10
# endif
# ifndef EAI_OVERFLOW
#  define EAI_OVERFLOW -11
# endif
# ifndef EAI_SYSTEM
#  define EAI_SYSTEM -12
# endif

# ifndef NI_NUMERICHOST
#  define NI_NUMERICHOST 0x01
#  define NI_NUMERICSERV 0x02
#  define NI_NOFQDN      0x04
#  define NI_NAMEREQD    0x08
#  define NI_DGRAM       0x10
# endif

# ifndef NI_MAXHOST
#  define NI_MAXHOST 1025
#  define NI_MAXSERV 32
# endif

# define AI_PASSIVE     1
# define AI_CANONNAME   2
# define AI_NUMERICHOST 4

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

# ifdef __LIBCN__
/* OS/2 LIBCn has inet_pton(). Because of this, socklen_t is not defined above.
 * And OS/2 LIBCn has socklen_t. So include sys/socket.h here for socklen_t. */
#  include <sys/socket.h>
# endif

const char *gai_strerror (int);

int  getaddrinfo  (const char *node, const char *service,
                   const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo (struct addrinfo *res);
int  getnameinfo  (const struct sockaddr *sa, socklen_t salen,
                   char *host, int hostlen, char *serv, int servlen,
                   int flags);

/* OS/2 does not support IPv6, yet. But declare these only for compilation */
# include <stdint.h>

struct in6_addr
{
    uint8_t s6_addr[16];
};

struct sockaddr_in6
{
    uint8_t         sin6_len;
    uint8_t         sin6_family;
    uint16_t        sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};

# define IN6_IS_ADDR_MULTICAST(a)   (((__const uint8_t *) (a))[0] == 0xff)

# define INET6_ADDRSTRLEN   46

static const struct in6_addr in6addr_any =
    { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };

#define IN6ADDR_ANY_INIT \
    { { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } }

# include <errno.h>
# ifndef EPROTO
#  define EPROTO (ELAST + 1)
# endif

# ifndef HAVE_IF_NAMETOINDEX
#  define if_nametoindex(name)  atoi(name)
# endif

/* static_assert missing in assert.h */
# if defined(__STDC_VERSION__) && \
     __STDC_VERSION__ >= 201112L && __STDC_VERSION__ < 202311L
#  include <assert.h>
#  ifndef static_assert
#   define static_assert _Static_assert
#  endif
# endif
#endif  /* __OS2__ */

/* math.h */

#ifndef HAVE_NANF
#define nanf(tagp) NAN
#endif

#ifndef HAVE_SINCOS
void sincos(double, double *, double *);
void sincosf(float, float *, float *);
#endif

#ifndef HAVE_REALPATH
char *realpath(const char * restrict pathname, char * restrict resolved_path);
#endif

/* mingw-w64 has a broken IN6_IS_ADDR_MULTICAST macro */
#if defined(_WIN32) && defined(__MINGW64_VERSION_MAJOR)
# define IN6_IS_ADDR_MULTICAST IN6_IS_ADDR_MULTICAST
#endif

#ifdef __APPLE__
# define fdatasync fsync

# ifdef __cplusplus
}
# endif
# include <time.h>
# ifdef __cplusplus
extern "C" {
# endif
# ifndef TIMER_ABSTIME
#  define TIMER_ABSTIME 0x01
# endif
# ifndef CLOCK_REALTIME
#  define CLOCK_REALTIME 0
# endif
# ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC 6
# endif
# ifndef HAVE_CLOCK_GETTIME
int clock_gettime(clockid_t clock_id, struct timespec *tp);
# endif
# ifndef HAVE_CLOCK_GETRES
int clock_getres(clockid_t clock_id, struct timespec *tp);
# endif
#endif

#ifndef _WIN32
# ifndef HAVE_CLOCK_NANOSLEEP
#  ifdef __cplusplus
}
#  endif
# include <time.h>
#  ifdef __cplusplus
extern "C" {
#  endif
int clock_nanosleep(clockid_t clock_id, int flags,
        const struct timespec *rqtp, struct timespec *rmtp);
# endif
#endif

#ifdef _WIN32
# if defined(_CRT_INTERNAL_NONSTDC_NAMES) && !_CRT_INTERNAL_NONSTDC_NAMES
#  include <string.h>
// the MS POSIX aliases are missing
static inline char *strdup(const char *str)
{
    return _strdup(str);
}

#  define O_WRONLY    _O_WRONLY
#  define O_CREAT     _O_CREAT
#  define O_APPEND    _O_APPEND
#  define O_TRUNC     _O_TRUNC
#  define O_BINARY    _O_BINARY
#  define O_EXCL      _O_EXCL
#  define O_RDWR      _O_RDWR
#  define O_TEXT      _O_TEXT
#  define O_NOINHERIT _O_NOINHERIT
#  define O_RDONLY    _O_RDONLY

# endif // !_CRT_INTERNAL_NONSTDC_NAMES
#endif // _WIN32


#ifdef __cplusplus
} /* extern "C" */
#endif

#if defined(__cplusplus)
#ifndef HAVE_CXX_TYPEOF
# include <type_traits>
# define typeof(t) std::remove_reference<decltype(t)>::type
#endif
#endif

#endif /* !LIBVLC_FIXUPS_H */
