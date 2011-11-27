/*****************************************************************************
 * vlc_threads.h : threads implementation for the VideoLAN client
 * This header provides portable declarations for mutexes & conditions
 *****************************************************************************
 * Copyright (C) 1999, 2002 VLC authors and VideoLAN
 * Copyright © 2007-2008 Rémi Denis-Courmont
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef VLC_THREADS_H_
#define VLC_THREADS_H_

/**
 * \file
 * This file defines structures and functions for handling threads in vlc
 *
 */

#if defined( UNDER_CE )
#elif defined( WIN32 )
#   include <process.h>                                         /* Win32 API */

#elif defined( __OS2__ )                                        /* OS/2 API  */
#   include <errno.h>

#   define pthread_sigmask  sigprocmask

#else                                         /* pthreads (like Linux & BSD) */
#   define LIBVLC_USE_PTHREAD 1
#   define LIBVLC_USE_PTHREAD_CANCEL 1
#   define _APPLE_C_SOURCE    1 /* Proper pthread semantics on OSX */

#   include <unistd.h> /* _POSIX_SPIN_LOCKS */
#   include <pthread.h>

/* Unnamed POSIX semaphores not supported on Mac OS X, use Mach semaphores instead */
#   if defined (__APPLE__)
#      include <mach/semaphore.h>
#      include <mach/task.h>
#   else
#      include <semaphore.h>
#   endif

#endif

/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Thread priorities */
#ifdef __APPLE__
#   define VLC_THREAD_PRIORITY_LOW      0
#   define VLC_THREAD_PRIORITY_INPUT   22
#   define VLC_THREAD_PRIORITY_AUDIO   22
#   define VLC_THREAD_PRIORITY_VIDEO    0
#   define VLC_THREAD_PRIORITY_OUTPUT  22
#   define VLC_THREAD_PRIORITY_HIGHEST 22

#elif defined(LIBVLC_USE_PTHREAD)
#   define VLC_THREAD_PRIORITY_LOW      0
#   define VLC_THREAD_PRIORITY_INPUT   10
#   define VLC_THREAD_PRIORITY_AUDIO    5
#   define VLC_THREAD_PRIORITY_VIDEO    0
#   define VLC_THREAD_PRIORITY_OUTPUT  15
#   define VLC_THREAD_PRIORITY_HIGHEST 20

#elif defined(WIN32) || defined(UNDER_CE)
/* Define different priorities for WinNT/2K/XP and Win9x/Me */
#   define VLC_THREAD_PRIORITY_LOW 0
#   define VLC_THREAD_PRIORITY_INPUT \
        THREAD_PRIORITY_ABOVE_NORMAL
#   define VLC_THREAD_PRIORITY_AUDIO \
        THREAD_PRIORITY_HIGHEST
#   define VLC_THREAD_PRIORITY_VIDEO 0
#   define VLC_THREAD_PRIORITY_OUTPUT \
        THREAD_PRIORITY_ABOVE_NORMAL
#   define VLC_THREAD_PRIORITY_HIGHEST \
        THREAD_PRIORITY_TIME_CRITICAL

#elif defined(__OS2__)
#   define VLC_THREAD_PRIORITY_LOW      0
#   define VLC_THREAD_PRIORITY_INPUT    MAKESHORT( PRTYD_MAXIMUM / 2, PRTYC_REGULAR )
#   define VLC_THREAD_PRIORITY_AUDIO    MAKESHORT( PRTYD_MAXIMUM, PRTYC_REGULAR )
#   define VLC_THREAD_PRIORITY_VIDEO    0
#   define VLC_THREAD_PRIORITY_OUTPUT   MAKESHORT( PRTYD_MAXIMUM / 2, PRTYC_REGULAR )
#   define VLC_THREAD_PRIORITY_HIGHEST  MAKESHORT( 0, PRTYC_TIMECRITICAL )

#else
#   define VLC_THREAD_PRIORITY_LOW 0
#   define VLC_THREAD_PRIORITY_INPUT 0
#   define VLC_THREAD_PRIORITY_AUDIO 0
#   define VLC_THREAD_PRIORITY_VIDEO 0
#   define VLC_THREAD_PRIORITY_OUTPUT 0
#   define VLC_THREAD_PRIORITY_HIGHEST 0

#endif

/*****************************************************************************
 * Type definitions
 *****************************************************************************/

#if defined (LIBVLC_USE_PTHREAD)
typedef pthread_t       vlc_thread_t;
typedef pthread_mutex_t vlc_mutex_t;
#define VLC_STATIC_MUTEX PTHREAD_MUTEX_INITIALIZER
typedef pthread_cond_t  vlc_cond_t;
#define VLC_STATIC_COND  PTHREAD_COND_INITIALIZER
typedef pthread_rwlock_t vlc_rwlock_t;
#define VLC_STATIC_RWLOCK PTHREAD_RWLOCK_INITIALIZER
typedef pthread_key_t   vlc_threadvar_t;
typedef struct vlc_timer *vlc_timer_t;

#if defined (__APPLE__)
typedef semaphore_t     vlc_sem_t;
#else
typedef sem_t           vlc_sem_t;
#endif

#elif defined( WIN32 )
typedef struct vlc_thread *vlc_thread_t;

typedef struct
{
    bool dynamic;
    union
    {
        struct
        {
            bool locked;
            unsigned long contention;
        };
        CRITICAL_SECTION mutex;
    };
} vlc_mutex_t;
#define VLC_STATIC_MUTEX { false, { { false, 0 } } }

typedef struct
{
    HANDLE   handle;
    unsigned clock;
} vlc_cond_t;
#define VLC_STATIC_COND { 0, 0 }

typedef HANDLE  vlc_sem_t;

typedef struct
{
    vlc_mutex_t   mutex;
    vlc_cond_t    wait;
    unsigned long readers;
    unsigned long writers;
    DWORD         writer;
} vlc_rwlock_t;
#define VLC_STATIC_RWLOCK \
    { VLC_STATIC_MUTEX, VLC_STATIC_COND, 0, 0, 0 }

typedef struct vlc_threadvar *vlc_threadvar_t;
typedef struct vlc_timer *vlc_timer_t;

#elif defined( __OS2__ )
typedef struct vlc_thread *vlc_thread_t;

typedef struct
{
    bool dynamic;
    union
    {
        struct
        {
            bool locked;
            unsigned long contention;
        };
        HMTX hmtx;
    };
} vlc_mutex_t;

#define VLC_STATIC_MUTEX { false, { { false, 0 } } }

typedef struct
{
    HEV      hev;
    unsigned clock;
} vlc_cond_t;

#define VLC_STATIC_COND { 0, 0 }

typedef struct
{
    HEV  hev;
    HMTX wait_mutex;
    HMTX count_mutex;
    int  count;
} vlc_sem_t;

typedef struct
{
    vlc_mutex_t   mutex;
    vlc_cond_t    wait;
    unsigned long readers;
    unsigned long writers;
    int           writer;
} vlc_rwlock_t;
#define VLC_STATIC_RWLOCK \
    { VLC_STATIC_MUTEX, VLC_STATIC_COND, 0, 0, 0 }

typedef struct vlc_threadvar *vlc_threadvar_t;
typedef struct vlc_timer *vlc_timer_t;

#endif

#if defined( WIN32 ) && !defined ETIMEDOUT
#  define ETIMEDOUT 10060 /* This is the value in winsock.h. */
#endif

/*****************************************************************************
 * Function definitions
 *****************************************************************************/
VLC_API void vlc_mutex_init( vlc_mutex_t * );
VLC_API void vlc_mutex_init_recursive( vlc_mutex_t * );
VLC_API void vlc_mutex_destroy( vlc_mutex_t * );
VLC_API void vlc_mutex_lock( vlc_mutex_t * );
VLC_API int vlc_mutex_trylock( vlc_mutex_t * ) VLC_USED;
VLC_API void vlc_mutex_unlock( vlc_mutex_t * );
VLC_API void vlc_cond_init( vlc_cond_t * );
VLC_API void vlc_cond_init_daytime( vlc_cond_t * );
VLC_API void vlc_cond_destroy( vlc_cond_t * );
VLC_API void vlc_cond_signal(vlc_cond_t *);
VLC_API void vlc_cond_broadcast(vlc_cond_t *);
VLC_API void vlc_cond_wait(vlc_cond_t *, vlc_mutex_t *);
VLC_API int vlc_cond_timedwait(vlc_cond_t *, vlc_mutex_t *, mtime_t);
VLC_API void vlc_sem_init(vlc_sem_t *, unsigned);
VLC_API void vlc_sem_destroy(vlc_sem_t *);
VLC_API int vlc_sem_post(vlc_sem_t *);
VLC_API void vlc_sem_wait(vlc_sem_t *);

VLC_API void vlc_rwlock_init(vlc_rwlock_t *);
VLC_API void vlc_rwlock_destroy(vlc_rwlock_t *);
VLC_API void vlc_rwlock_rdlock(vlc_rwlock_t *);
VLC_API void vlc_rwlock_wrlock(vlc_rwlock_t *);
VLC_API void vlc_rwlock_unlock(vlc_rwlock_t *);
VLC_API int vlc_threadvar_create(vlc_threadvar_t * , void (*) (void *) );
VLC_API void vlc_threadvar_delete(vlc_threadvar_t *);
VLC_API int vlc_threadvar_set(vlc_threadvar_t, void *);
VLC_API void * vlc_threadvar_get(vlc_threadvar_t);

VLC_API int vlc_clone(vlc_thread_t *, void * (*) (void *), void *, int) VLC_USED;
VLC_API void vlc_cancel(vlc_thread_t);
VLC_API void vlc_join(vlc_thread_t, void **);
VLC_API void vlc_control_cancel (int cmd, ...);

VLC_API mtime_t mdate(void);
VLC_API void mwait(mtime_t deadline);
VLC_API void msleep(mtime_t delay);

#define VLC_HARD_MIN_SLEEP   10000 /* 10 milliseconds = 1 tick at 100Hz */
#define VLC_SOFT_MIN_SLEEP 9000000 /* 9 seconds */

#if VLC_GCC_VERSION(4,3)
/* Linux has 100, 250, 300 or 1000Hz
 *
 * HZ=100 by default on FreeBSD, but some architectures use a 1000Hz timer
 */

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((error("sorry, cannot sleep for such short a time")))
mtime_t impossible_delay( mtime_t delay )
{
    (void) delay;
    return VLC_HARD_MIN_SLEEP;
}

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((warning("use proper event handling instead of short delay")))
mtime_t harmful_delay( mtime_t delay )
{
    return delay;
}

# define check_delay( d ) \
    ((__builtin_constant_p(d < VLC_HARD_MIN_SLEEP) \
   && (d < VLC_HARD_MIN_SLEEP)) \
       ? impossible_delay(d) \
       : ((__builtin_constant_p(d < VLC_SOFT_MIN_SLEEP) \
       && (d < VLC_SOFT_MIN_SLEEP)) \
           ? harmful_delay(d) \
           : d))

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((error("deadlines can not be constant")))
mtime_t impossible_deadline( mtime_t deadline )
{
    return deadline;
}

# define check_deadline( d ) \
    (__builtin_constant_p(d) ? impossible_deadline(d) : d)
#else
# define check_delay(d) (d)
# define check_deadline(d) (d)
#endif

#define msleep(d) msleep(check_delay(d))
#define mwait(d) mwait(check_deadline(d))

VLC_API int vlc_timer_create(vlc_timer_t *, void (*) (void *), void *) VLC_USED;
VLC_API void vlc_timer_destroy(vlc_timer_t);
VLC_API void vlc_timer_schedule(vlc_timer_t, bool, mtime_t, mtime_t);
VLC_API unsigned vlc_timer_getoverrun(vlc_timer_t) VLC_USED;

VLC_API unsigned vlc_GetCPUCount(void);

#ifndef LIBVLC_USE_PTHREAD_CANCEL
enum {
    VLC_CLEANUP_PUSH,
    VLC_CLEANUP_POP,
};
#endif

VLC_API int vlc_savecancel(void);
VLC_API void vlc_restorecancel(int state);
VLC_API void vlc_testcancel(void);

#if defined (LIBVLC_USE_PTHREAD_CANCEL)
/**
 * Registers a new procedure to run if the thread is cancelled (or otherwise
 * exits prematurely). Any call to vlc_cleanup_push() <b>must</b> paired with a
 * call to either vlc_cleanup_pop() or vlc_cleanup_run(). Branching into or out
 * of the block between these two function calls is not allowed (read: it will
 * likely crash the whole process). If multiple procedures are registered,
 * they are handled in last-in first-out order.
 *
 * @param routine procedure to call if the thread ends
 * @param arg argument for the procedure
 */
# define vlc_cleanup_push( routine, arg ) pthread_cleanup_push (routine, arg)

/**
 * Removes a cleanup procedure that was previously registered with
 * vlc_cleanup_push().
 */
# define vlc_cleanup_pop( ) pthread_cleanup_pop (0)

/**
 * Removes a cleanup procedure that was previously registered with
 * vlc_cleanup_push(), and executes it.
 */
# define vlc_cleanup_run( ) pthread_cleanup_pop (1)
#else
typedef struct vlc_cleanup_t vlc_cleanup_t;

struct vlc_cleanup_t
{
    vlc_cleanup_t *next;
    void         (*proc) (void *);
    void          *data;
};

/* This macros opens a code block on purpose. This is needed for multiple
 * calls within a single function. This also prevent Win32 developers from
 * writing code that would break on POSIX (POSIX opens a block as well). */
# define vlc_cleanup_push( routine, arg ) \
    do { \
        vlc_cleanup_t vlc_cleanup_data = { NULL, routine, arg, }; \
        vlc_control_cancel (VLC_CLEANUP_PUSH, &vlc_cleanup_data)

# define vlc_cleanup_pop( ) \
        vlc_control_cancel (VLC_CLEANUP_POP); \
    } while (0)

# define vlc_cleanup_run( ) \
        vlc_control_cancel (VLC_CLEANUP_POP); \
        vlc_cleanup_data.proc (vlc_cleanup_data.data); \
    } while (0)

#endif /* LIBVLC_USE_PTHREAD_CANCEL */

static inline void vlc_cleanup_lock (void *lock)
{
    vlc_mutex_unlock ((vlc_mutex_t *)lock);
}
#define mutex_cleanup_push( lock ) vlc_cleanup_push (vlc_cleanup_lock, lock)

# if defined (_POSIX_SPIN_LOCKS) && ((_POSIX_SPIN_LOCKS - 0) > 0)
typedef pthread_spinlock_t vlc_spinlock_t;

/**
 * Initializes a spinlock.
 */
static inline void vlc_spin_init (vlc_spinlock_t *spin)
{
    if (pthread_spin_init (spin, PTHREAD_PROCESS_PRIVATE))
        abort ();
}

/**
 * Acquires a spinlock.
 */
static inline void vlc_spin_lock (vlc_spinlock_t *spin)
{
    pthread_spin_lock (spin);
}

/**
 * Releases a spinlock.
 */
static inline void vlc_spin_unlock (vlc_spinlock_t *spin)
{
    pthread_spin_unlock (spin);
}

/**
 * Deinitializes a spinlock.
 */
static inline void vlc_spin_destroy (vlc_spinlock_t *spin)
{
    pthread_spin_destroy (spin);
}

#elif defined (WIN32) && !defined (UNDER_CE)

typedef CRITICAL_SECTION vlc_spinlock_t;

/**
 * Initializes a spinlock.
 */
static inline void vlc_spin_init (vlc_spinlock_t *spin)
{
    if (!InitializeCriticalSectionAndSpinCount(spin, 4000))
        abort ();
}

/**
 * Acquires a spinlock.
 */
static inline void vlc_spin_lock (vlc_spinlock_t *spin)
{
    EnterCriticalSection(spin);
}

/**
 * Releases a spinlock.
 */
static inline void vlc_spin_unlock (vlc_spinlock_t *spin)
{
    LeaveCriticalSection(spin);
}

/**
 * Deinitializes a spinlock.
 */
static inline void vlc_spin_destroy (vlc_spinlock_t *spin)
{
    DeleteCriticalSection(spin);
}

#else

/* Fallback to plain mutexes if spinlocks are not available */
typedef vlc_mutex_t vlc_spinlock_t;

static inline void vlc_spin_init (vlc_spinlock_t *spin)
{
    vlc_mutex_init (spin);
}

# define vlc_spin_lock    vlc_mutex_lock
# define vlc_spin_unlock  vlc_mutex_unlock
# define vlc_spin_destroy vlc_mutex_destroy
#endif

/**
 * Issues a full memory barrier.
 */
#if defined (__APPLE__)
# include <libkern/OSAtomic.h> /* OSMemoryBarrier() */
#endif
static inline void barrier (void)
{
#if defined (__GNUC__) && !defined (__APPLE__) && \
            ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))
    __sync_synchronize ();
#elif defined(__APPLE__)
    OSMemoryBarrier ();
#elif defined(__powerpc__)
    asm volatile ("sync":::"memory");
#elif 0 // defined(__i386__) /*  Requires SSE2 support */
    asm volatile ("mfence":::"memory");
#else
    vlc_spinlock_t spin;
    vlc_spin_init (&spin);
    vlc_spin_lock (&spin);
    vlc_spin_unlock (&spin);
    vlc_spin_destroy (&spin);
#endif
}

#ifdef __cplusplus
/**
 * Helper C++ class to lock a mutex.
 * The mutex is locked when the object is created, and unlocked when the object
 * is destroyed.
 */
class vlc_mutex_locker
{
    private:
        vlc_mutex_t *lock;
    public:
        vlc_mutex_locker (vlc_mutex_t *m) : lock (m)
        {
            vlc_mutex_lock (lock);
        }

        ~vlc_mutex_locker (void)
        {
            vlc_mutex_unlock (lock);
        }
};
#endif

enum {
   VLC_AVCODEC_MUTEX = 0,
   VLC_GCRYPT_MUTEX,
   VLC_XLIB_MUTEX,
   VLC_MOSAIC_MUTEX,
   VLC_HIGHLIGHT_MUTEX,
   /* Insert new entry HERE */
   VLC_MAX_MUTEX
};

VLC_API void vlc_global_mutex( unsigned, bool );
#define vlc_global_lock( n ) vlc_global_mutex( n, true )
#define vlc_global_unlock( n ) vlc_global_mutex( n, false )

#endif /* !_VLC_THREADS_H */
