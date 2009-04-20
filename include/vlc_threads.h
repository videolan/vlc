/*****************************************************************************
 * vlc_threads.h : threads implementation for the VideoLAN client
 * This header provides portable declarations for mutexes & conditions
 *****************************************************************************
 * Copyright (C) 1999, 2002 the VideoLAN team
 * Copyright © 2007-2008 Rémi Denis-Courmont
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef VLC_THREADS_H_
#define VLC_THREADS_H_

/**
 * \file
 * This file defines structures and functions for handling threads in vlc
 *
 */

#if defined( UNDER_CE )
#   include <errno.h>                                           /* WinCE API */
#elif defined( WIN32 )
#   include <process.h>                                         /* Win32 API */
#   include <errno.h>

#else                                         /* pthreads (like Linux & BSD) */
#   define LIBVLC_USE_PTHREAD 1
#   define LIBVLC_USE_PTHREAD_CANCEL 1
#   define _APPLE_C_SOURCE    1 /* Proper pthread semantics on OSX */

#   include <stdlib.h> /* lldiv_t definition (only in C99) */
#   include <unistd.h> /* _POSIX_SPIN_LOCKS */
#   include <pthread.h>
    /* Needed for pthread_cond_timedwait */
#   include <errno.h>
#   include <time.h>

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
typedef pthread_key_t   vlc_threadvar_t;

#elif defined( WIN32 )
typedef struct
{
    HANDLE handle;
    void  *(*entry) (void *);
    void  *data;
#if defined( UNDER_CE )
    HANDLE cancel_event;
#endif
} *vlc_thread_t;

typedef struct
{
    LONG initialized;
    CRITICAL_SECTION mutex;
} vlc_mutex_t;
#define VLC_STATIC_MUTEX { 0, }

typedef HANDLE  vlc_cond_t;
typedef DWORD   vlc_threadvar_t;

#endif

#if defined( WIN32 ) && !defined ETIMEDOUT
#  define ETIMEDOUT 10060 /* This is the value in winsock.h. */
#endif

/*****************************************************************************
 * Function definitions
 *****************************************************************************/
VLC_EXPORT( int,  vlc_mutex_init,    ( vlc_mutex_t * ) );
VLC_EXPORT( int,  vlc_mutex_init_recursive, ( vlc_mutex_t * ) );
VLC_EXPORT( void, vlc_mutex_destroy, ( vlc_mutex_t * ) );
VLC_EXPORT( void, vlc_mutex_lock, ( vlc_mutex_t * ) );
VLC_EXPORT( int, vlc_mutex_trylock, ( vlc_mutex_t * ) );
VLC_EXPORT( void, vlc_mutex_unlock, ( vlc_mutex_t * ) );
VLC_EXPORT( int,  vlc_cond_init,     ( vlc_cond_t * ) );
VLC_EXPORT( void, vlc_cond_destroy,  ( vlc_cond_t * ) );
VLC_EXPORT( void, vlc_cond_signal, (vlc_cond_t *) );
VLC_EXPORT( void, vlc_cond_broadcast, (vlc_cond_t *) );
VLC_EXPORT( void, vlc_cond_wait, (vlc_cond_t *, vlc_mutex_t *) );
VLC_EXPORT( int, vlc_cond_timedwait, (vlc_cond_t *, vlc_mutex_t *, mtime_t) );
VLC_EXPORT( int, vlc_threadvar_create, (vlc_threadvar_t * , void (*) (void *) ) );
VLC_EXPORT( void, vlc_threadvar_delete, (vlc_threadvar_t *) );
VLC_EXPORT( int, vlc_threadvar_set, (vlc_threadvar_t, void *) );
VLC_EXPORT( void *, vlc_threadvar_get, (vlc_threadvar_t) );
VLC_EXPORT( int,  vlc_thread_create, ( vlc_object_t *, const char *, int, const char *, void * ( * ) ( vlc_object_t * ), int ) );
VLC_EXPORT( int,  __vlc_thread_set_priority, ( vlc_object_t *, const char *, int, int ) );
VLC_EXPORT( void, __vlc_thread_join,   ( vlc_object_t * ) );

VLC_EXPORT( int, vlc_clone, (vlc_thread_t *, void * (*) (void *), void *, int) );
VLC_EXPORT( void, vlc_cancel, (vlc_thread_t) );
VLC_EXPORT( void, vlc_join, (vlc_thread_t, void **) );
VLC_EXPORT (void, vlc_control_cancel, (int cmd, ...));

#ifndef LIBVLC_USE_PTHREAD_CANCEL
enum {
    VLC_DO_CANCEL,
    VLC_CLEANUP_PUSH,
    VLC_CLEANUP_POP,
};
#endif

VLC_EXPORT( int, vlc_savecancel, (void) );
VLC_EXPORT( void, vlc_restorecancel, (int state) );
VLC_EXPORT( void, vlc_testcancel, (void) );

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
 * calls within a single function. This also prevent Win32 developpers from
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
static inline int vlc_spin_init (vlc_spinlock_t *spin)
{
    return pthread_spin_init (spin, PTHREAD_PROCESS_PRIVATE);
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

#elif defined( WIN32 )

typedef CRITICAL_SECTION vlc_spinlock_t;

/**
 * Initializes a spinlock.
 */
static inline int vlc_spin_init (vlc_spinlock_t *spin)
{
#ifdef UNDER_CE
    InitializeCriticalSection(spin);
    return 0;
#else
    return !InitializeCriticalSectionAndSpinCount(spin, 4000);
#endif
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

static inline int vlc_spin_init (vlc_spinlock_t *spin)
{
    return vlc_mutex_init (spin);
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

/*****************************************************************************
 * vlc_thread_create: create a thread
 *****************************************************************************/
#define vlc_thread_create( P_THIS, PSZ_NAME, FUNC, PRIORITY )         \
    vlc_thread_create( VLC_OBJECT(P_THIS), __FILE__, __LINE__, PSZ_NAME, FUNC, PRIORITY )

/*****************************************************************************
 * vlc_thread_set_priority: set the priority of the calling thread
 *****************************************************************************/
#define vlc_thread_set_priority( P_THIS, PRIORITY )                         \
    __vlc_thread_set_priority( VLC_OBJECT(P_THIS), __FILE__, __LINE__, PRIORITY )

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits
 *****************************************************************************/
#define vlc_thread_join( P_THIS )                                           \
    __vlc_thread_join( VLC_OBJECT(P_THIS) )

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

#endif /* !_VLC_THREADS_H */
