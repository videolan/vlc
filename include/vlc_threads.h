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
                                                                /* WinCE API */
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
        (IS_WINNT ? THREAD_PRIORITY_ABOVE_NORMAL : 0)
#   define VLC_THREAD_PRIORITY_AUDIO \
        (IS_WINNT ? THREAD_PRIORITY_HIGHEST : 0)
#   define VLC_THREAD_PRIORITY_VIDEO \
        (IS_WINNT ? 0 : THREAD_PRIORITY_BELOW_NORMAL )
#   define VLC_THREAD_PRIORITY_OUTPUT \
        (IS_WINNT ? THREAD_PRIORITY_ABOVE_NORMAL : 0)
#   define VLC_THREAD_PRIORITY_HIGHEST \
        (IS_WINNT ? THREAD_PRIORITY_TIME_CRITICAL : 0)

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
typedef pthread_cond_t  vlc_cond_t;
typedef pthread_key_t   vlc_threadvar_t;

#elif defined( WIN32 ) || defined( UNDER_CE )
typedef struct
{
    HANDLE handle;
    void  *(*entry) (void *);
    void  *data;
} *vlc_thread_t;

typedef HANDLE  vlc_mutex_t;
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
VLC_EXPORT( void,  __vlc_mutex_destroy, ( const char *, int, vlc_mutex_t * ) );
VLC_EXPORT( int,  vlc_cond_init,     ( vlc_cond_t * ) );
VLC_EXPORT( void,  __vlc_cond_destroy,  ( const char *, int, vlc_cond_t * ) );
VLC_EXPORT( int, vlc_threadvar_create, (vlc_threadvar_t * , void (*) (void *) ) );
VLC_EXPORT( void, vlc_threadvar_delete, (vlc_threadvar_t *) );
VLC_EXPORT( int,  __vlc_thread_create, ( vlc_object_t *, const char *, int, const char *, void * ( * ) ( vlc_object_t * ), int, bool ) );
VLC_EXPORT( int,  __vlc_thread_set_priority, ( vlc_object_t *, const char *, int, int ) );
VLC_EXPORT( void, __vlc_thread_join,   ( vlc_object_t * ) );

VLC_EXPORT( int, vlc_clone, (vlc_thread_t *, void * (*) (void *), void *, int) );
VLC_EXPORT( void, vlc_cancel, (vlc_thread_t) );
VLC_EXPORT( void, vlc_join, (vlc_thread_t, void **) );
VLC_EXPORT (void, vlc_control_cancel, (int cmd, ...));

#ifndef LIBVLC_USE_PTHREAD_CANCEL
enum {
    VLC_SAVE_CANCEL,
    VLC_RESTORE_CANCEL,
    VLC_TEST_CANCEL,
    VLC_DO_CANCEL,
    VLC_CLEANUP_PUSH,
    VLC_CLEANUP_POP,
};
#endif

#define vlc_thread_ready vlc_object_signal

/*****************************************************************************
 * vlc_mutex_lock: lock a mutex
 *****************************************************************************/
#define vlc_mutex_lock( P_MUTEX )                                           \
    __vlc_mutex_lock( __FILE__, __LINE__, P_MUTEX )

VLC_EXPORT(void, vlc_thread_fatal, (const char *action, int error, const char *function, const char *file, unsigned line));

#if defined(LIBVLC_USE_PTHREAD)
# define VLC_THREAD_ASSERT( action ) \
    if (val) \
        vlc_thread_fatal (action, val, __func__, psz_file, i_line)
#else
# define VLC_THREAD_ASSERT ((void)(val))
#endif

static inline void __vlc_mutex_lock( const char * psz_file, int i_line,
                                    vlc_mutex_t * p_mutex )
{
#if defined(LIBVLC_USE_PTHREAD)
#   define vlc_assert_locked( m ) \
           assert (pthread_mutex_lock (m) == EDEADLK)
    int val = pthread_mutex_lock( p_mutex );
    VLC_THREAD_ASSERT ("locking mutex");

#elif defined( UNDER_CE )
    (void)psz_file; (void)i_line;

    EnterCriticalSection( &p_mutex->csection );

#elif defined( WIN32 )
    (void)psz_file; (void)i_line;

    WaitForSingleObject( *p_mutex, INFINITE );

#endif
}

#ifndef vlc_assert_locked
# define vlc_assert_locked( m ) (void)m
#endif

/*****************************************************************************
 * vlc_mutex_unlock: unlock a mutex
 *****************************************************************************/
#define vlc_mutex_unlock( P_MUTEX )                                         \
    __vlc_mutex_unlock( __FILE__, __LINE__, P_MUTEX )

static inline void __vlc_mutex_unlock( const char * psz_file, int i_line,
                                      vlc_mutex_t *p_mutex )
{
#if defined(LIBVLC_USE_PTHREAD)
    int val = pthread_mutex_unlock( p_mutex );
    VLC_THREAD_ASSERT ("unlocking mutex");

#elif defined( UNDER_CE )
    (void)psz_file; (void)i_line;

    LeaveCriticalSection( &p_mutex->csection );

#elif defined( WIN32 )
    (void)psz_file; (void)i_line;

    ReleaseMutex( *p_mutex );

#endif
}

/*****************************************************************************
 * vlc_mutex_destroy: destroy a mutex
 *****************************************************************************/
#define vlc_mutex_destroy( P_MUTEX )                                        \
    __vlc_mutex_destroy( __FILE__, __LINE__, P_MUTEX )

/**
 * Save the cancellation state and disable cancellation for the calling thread.
 * This function must be called before entering a piece of code that is not
 * cancellation-safe.
 * @return Previous cancellation state (opaque value).
 */
static inline int vlc_savecancel (void)
{
    int state;
#if defined (LIBVLC_USE_PTHREAD_CANCEL)
    (void) pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);
#else
    vlc_control_cancel (VLC_SAVE_CANCEL, &state);
#endif
    return state;
}

/**
 * Restore the cancellation state for the calling thread.
 * @param state previous state as returned by vlc_savecancel().
 * @return Nothing, always succeeds.
 */
static inline void vlc_restorecancel (int state)
{
#if defined (LIBVLC_USE_PTHREAD_CANCEL)
    (void) pthread_setcancelstate (state, NULL);
#else
    vlc_control_cancel (VLC_RESTORE_CANCEL, state);
#endif
}

/**
 * Issues an explicit deferred cancellation point.
 * This has no effect if thread cancellation is disabled.
 * This can be called when there is a rather slow non-sleeping operation.
 */
static inline void vlc_testcancel (void)
{
#if defined (LIBVLC_USE_PTHREAD_CANCEL)
    pthread_testcancel ();
#else
    vlc_control_cancel (VLC_TEST_CANCEL);
#endif
}

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

/*****************************************************************************
 * vlc_cond_signal: start a thread on condition completion
 *****************************************************************************/
#define vlc_cond_signal( P_COND )                                           \
    __vlc_cond_signal( __FILE__, __LINE__, P_COND )

static inline void __vlc_cond_signal( const char * psz_file, int i_line,
                                      vlc_cond_t *p_condvar )
{
#if defined(LIBVLC_USE_PTHREAD)
    int val = pthread_cond_signal( p_condvar );
    VLC_THREAD_ASSERT ("signaling condition variable");

#elif defined( UNDER_CE ) || defined( WIN32 )
    (void)psz_file; (void)i_line;

    /* Release one waiting thread if one is available. */
    /* For this trick to work properly, the vlc_cond_signal must be surrounded
     * by a mutex. This will prevent another thread from stealing the signal */
    /* PulseEvent() only works if none of the waiting threads is suspended.
     * This is particularily problematic under a debug session.
     * as documented in http://support.microsoft.com/kb/q173260/ */
    PulseEvent( *p_condvar );

#endif
}

/*****************************************************************************
 * vlc_cond_wait: wait until condition completion
 *****************************************************************************/
#define vlc_cond_wait( P_COND, P_MUTEX )                                     \
    __vlc_cond_wait( __FILE__, __LINE__, P_COND, P_MUTEX  )

static inline void __vlc_cond_wait( const char * psz_file, int i_line,
                                    vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex )
{
#if defined(LIBVLC_USE_PTHREAD)
    int val = pthread_cond_wait( p_condvar, p_mutex );
    VLC_THREAD_ASSERT ("waiting on condition");

#elif defined( UNDER_CE )
    LeaveCriticalSection( &p_mutex->csection );
    WaitForSingleObject( *p_condvar, INFINITE );

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

#elif defined( WIN32 )
    do
        vlc_testcancel ();
    while (SignalObjectAndWait (*p_mutex, *p_condvar, INFINITE, TRUE)
            == WAIT_IO_COMPLETION);

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

    (void)psz_file; (void)i_line;

#endif
}


/*****************************************************************************
 * vlc_cond_timedwait: wait until condition completion or expiration
 *****************************************************************************
 * Returns 0 if object signaled, an error code in case of timeout or error.
 *****************************************************************************/
#define vlc_cond_timedwait( c, m, d ) \
    __vlc_cond_timedwait( __FILE__, __LINE__, c, m, check_deadline(d) )

static inline int __vlc_cond_timedwait( const char * psz_file, int i_line,
                                        vlc_cond_t *p_condvar,
                                        vlc_mutex_t *p_mutex,
                                        mtime_t deadline )
{
#if defined(LIBVLC_USE_PTHREAD)
    lldiv_t d = lldiv( deadline, CLOCK_FREQ );
    struct timespec ts = { d.quot, d.rem * (1000000000 / CLOCK_FREQ) };

    int val = pthread_cond_timedwait (p_condvar, p_mutex, &ts);
    if (val != ETIMEDOUT)
        VLC_THREAD_ASSERT ("timed-waiting on condition");
    return val;

#elif defined( UNDER_CE )
    mtime_t delay_ms = (deadline - mdate()) / (CLOCK_FREQ / 1000);
    DWORD result;
    if( delay_ms < 0 )
        delay_ms = 0;

    LeaveCriticalSection( &p_mutex->csection );
    result = WaitForSingleObject( *p_condvar, delay_ms );

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

    (void)psz_file; (void)i_line;

    return (result == WAIT_TIMEOUT) ? ETIMEDOUT : 0;

#elif defined( WIN32 )
    DWORD result;

    (void)psz_file; (void)i_line;

    do
    {
        vlc_testcancel ();

        mtime_t total = deadline - mdate ();
        if (total <= 0)
            break;
        DWORD delay = (total > 0x7fffffff) ? 0x7fffffff : total;
        result = SignalObjectAndWait (*p_mutex, *p_condvar, delay, TRUE);
    }
    while (result == WAIT_IO_COMPLETION);

    /* Reacquire the mutex before return/cancel. */
    vlc_mutex_lock (p_mutex);
    return (result == WAIT_OBJECT_0) ? 0 : ETIMEDOUT;

#endif
}

/*****************************************************************************
 * vlc_cond_destroy: destroy a condition
 *****************************************************************************/
#define vlc_cond_destroy( P_COND )                                          \
    __vlc_cond_destroy( __FILE__, __LINE__, P_COND )

/*****************************************************************************
 * vlc_threadvar_set: create: set the value of a thread-local variable
 *****************************************************************************/
static inline int vlc_threadvar_set( vlc_threadvar_t * p_tls, void *p_value )
{
    int i_ret;

#if defined(LIBVLC_USE_PTHREAD)
    i_ret = pthread_setspecific( *p_tls, p_value );

#elif defined( UNDER_CE ) || defined( WIN32 )
    i_ret = TlsSetValue( *p_tls, p_value ) ? EINVAL : 0;

#endif

    return i_ret;
}

/*****************************************************************************
 * vlc_threadvar_get: create: get the value of a thread-local variable
 *****************************************************************************/
static inline void* vlc_threadvar_get( vlc_threadvar_t * p_tls )
{
    void *p_ret;

#if defined(LIBVLC_USE_PTHREAD)
    p_ret = pthread_getspecific( *p_tls );

#elif defined( UNDER_CE ) || defined( WIN32 )
    p_ret = TlsGetValue( *p_tls );

#endif

    return p_ret;
}

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
    return !InitializeCriticalSectionAndSpinCount(spin, 4000);
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
#if defined (__GNUC__) && (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
    __sync_synchronize ();
#elif defined(__APPLE__)
    OSMemoryBarrier ();
#elif defined(__powerpc__)
    asm volatile ("sync":::"memory");
#elif defined(__i386__)
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
#define vlc_thread_create( P_THIS, PSZ_NAME, FUNC, PRIORITY, WAIT )         \
    __vlc_thread_create( VLC_OBJECT(P_THIS), __FILE__, __LINE__, PSZ_NAME, FUNC, PRIORITY, WAIT )

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

#endif /* !_VLC_THREADS_H */
