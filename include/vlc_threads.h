/*****************************************************************************
 * vlc_threads.h : threads implementation for the VideoLAN client
 * This header provides portable declarations for mutexes & conditions
 *****************************************************************************
 * Copyright (C) 1999, 2002 the VideoLAN team
 * $Id$
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

#ifndef _VLC_THREADS_H_
#define _VLC_THREADS_H_

#if defined( UNDER_CE )
                                                                /* WinCE API */
#elif defined( WIN32 )
#   include <process.h>                                         /* Win32 API */
#   include <errno.h>

#elif defined( SYS_BEOS )                                            /* BeOS */
#   include <kernel/OS.h>
#   include <kernel/scheduler.h>
#   include <byteorder.h>

#else                                         /* pthreads (like Linux & BSD) */
#   define LIBVLC_USE_PTHREAD 1
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

#elif defined(SYS_BEOS)
#   define VLC_THREAD_PRIORITY_LOW 5
#   define VLC_THREAD_PRIORITY_INPUT 10
#   define VLC_THREAD_PRIORITY_AUDIO 10
#   define VLC_THREAD_PRIORITY_VIDEO 5
#   define VLC_THREAD_PRIORITY_OUTPUT 15
#   define VLC_THREAD_PRIORITY_HIGHEST 15

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
typedef HANDLE  vlc_thread_t;
typedef BOOL (WINAPI *SIGNALOBJECTANDWAIT) ( HANDLE, HANDLE, DWORD, BOOL );
typedef HANDLE  vlc_mutex_t;
typedef HANDLE  vlc_cond_t;
typedef DWORD   vlc_threadvar_t;

#elif defined( SYS_BEOS )
/* This is the BeOS implementation of the vlc threads, note that the mutex is
 * not a real mutex and the cond_var is not like a pthread cond_var but it is
 * enough for what we need */

typedef thread_id vlc_thread_t;

typedef struct
{
    int32_t         init;
    sem_id          lock;
} vlc_mutex_t;

typedef struct
{
    int32_t         init;
    thread_id       thread;
} vlc_cond_t;

typedef struct
{
} vlc_threadvar_t;

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
VLC_EXPORT( int,  __vlc_cond_init,     ( vlc_cond_t * ) );
VLC_EXPORT( void,  __vlc_cond_destroy,  ( const char *, int, vlc_cond_t * ) );
VLC_EXPORT( int, vlc_threadvar_create, (vlc_threadvar_t * , void (*) (void *) ) );
VLC_EXPORT( void, vlc_threadvar_delete, (vlc_threadvar_t *) );
VLC_EXPORT( int,  __vlc_thread_create, ( vlc_object_t *, const char *, int, const char *, void * ( * ) ( vlc_object_t * ), int, bool ) );
VLC_EXPORT( int,  __vlc_thread_set_priority, ( vlc_object_t *, const char *, int, int ) );
VLC_EXPORT( void, __vlc_thread_join,   ( vlc_object_t *, const char *, int ) );

#define vlc_thread_ready vlc_object_signal

/*****************************************************************************
 * vlc_mutex_lock: lock a mutex
 *****************************************************************************/
#define vlc_mutex_lock( P_MUTEX )                                           \
    __vlc_mutex_lock( __FILE__, __LINE__, P_MUTEX )

VLC_EXPORT(void, vlc_pthread_fatal, (const char *action, int error, const char *file, unsigned line));

#if defined(LIBVLC_USE_PTHREAD)
# define VLC_THREAD_ASSERT( action ) \
    if (val) \
        vlc_pthread_fatal (action, val, psz_file, i_line)
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

#elif defined( SYS_BEOS )
    acquire_sem( p_mutex->lock );

#endif
}

#ifndef vlc_assert_locked
# define vlc_assert_locked( m ) (void)0
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

#elif defined( SYS_BEOS )
    release_sem( p_mutex->lock );

#endif
}

/*****************************************************************************
 * vlc_mutex_destroy: destroy a mutex
 *****************************************************************************/
#define vlc_mutex_destroy( P_MUTEX )                                        \
    __vlc_mutex_destroy( __FILE__, __LINE__, P_MUTEX )

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
#define vlc_cond_init( P_THIS, P_COND )                                     \
    __vlc_cond_init( P_COND )

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

#elif defined( SYS_BEOS )
    while( p_condvar->thread != -1 )
    {
        thread_info info;
        if( get_thread_info(p_condvar->thread, &info) == B_BAD_VALUE )
            return;

        if( info.state != B_THREAD_SUSPENDED )
        {
            /* The  waiting thread is not suspended so it could
             * have been interrupted beetwen the unlock and the
             * suspend_thread line. That is why we sleep a little
             * before retesting p_condver->thread. */
            snooze( 10000 );
        }
        else
        {
            /* Ok, we have to wake up that thread */
            resume_thread( p_condvar->thread );
        }
    }

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
    (void)psz_file; (void)i_line;

    /* Increase our wait count */
    SignalObjectAndWait( *p_mutex, *p_condvar, INFINITE, FALSE );

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

#elif defined( SYS_BEOS )
    /* The p_condvar->thread var is initialized before the unlock because
     * it enables to identify when the thread is interrupted beetwen the
     * unlock line and the suspend_thread line */
    p_condvar->thread = find_thread( NULL );
    vlc_mutex_unlock( p_mutex );
    suspend_thread( p_condvar->thread );
    p_condvar->thread = -1;

    vlc_mutex_lock( p_mutex );

#endif
}


/*****************************************************************************
 * vlc_cond_timedwait: wait until condition completion or expiration
 *****************************************************************************
 * Returns 0 if object signaled, an error code in case of timeout or error.
 *****************************************************************************/
#define vlc_cond_timedwait( P_COND, P_MUTEX, DEADLINE )                      \
    __vlc_cond_timedwait( __FILE__, __LINE__, P_COND, P_MUTEX, DEADLINE  )

static inline int __vlc_cond_timedwait( const char * psz_file, int i_line,
                                        vlc_cond_t *p_condvar,
                                        vlc_mutex_t *p_mutex,
                                        mtime_t deadline )
{
#if defined(LIBVLC_USE_PTHREAD)
    lldiv_t d = lldiv( deadline, 1000000 );
    struct timespec ts = { d.quot, d.rem * 1000 };

    int val = pthread_cond_timedwait (p_condvar, p_mutex, &ts);
    if (val == ETIMEDOUT)
        return ETIMEDOUT; /* this error is perfectly normal */
    VLC_THREAD_ASSERT ("timed-waiting on condition");

#elif defined( UNDER_CE )
    mtime_t delay_ms = (deadline - mdate())/1000;
    DWORD result;
    if( delay_ms < 0 )
        delay_ms = 0;

    LeaveCriticalSection( &p_mutex->csection );
    result = WaitForSingleObject( *p_condvar, delay_ms );

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

    if(result == WAIT_TIMEOUT)
       return ETIMEDOUT; /* this error is perfectly normal */

    (void)psz_file; (void)i_line;

#elif defined( WIN32 )
    mtime_t total = (deadline - mdate())/1000;
    DWORD result;
    if( total < 0 )
        total = 0;

    do
    {
        DWORD delay = (total > 0x7fffffff) ? 0x7fffffff : total;
        result = SignalObjectAndWait( *p_mutex, *p_condvar,
                                      delay, FALSE );
        total -= delay;
        vlc_mutex_lock (p_mutex);
    }
    while (total);

    /* Reacquire the mutex before returning. */
    if(result == WAIT_TIMEOUT)
       return ETIMEDOUT; /* this error is perfectly normal */

    (void)psz_file; (void)i_line;

#elif defined( SYS_BEOS )
#   error Unimplemented

#endif

    return 0;
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

#elif defined( SYS_BEOS )
    i_ret = EINVAL;

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

#elif defined( SYS_BEOS )
    p_ret = NULL;

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
#elif defined (LIBVLC_USE_PTHREAD)
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock (&lock);
    pthread_mutex_unlock (&lock);
#else
# error barrier not implemented!
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
    __vlc_thread_join( VLC_OBJECT(P_THIS), __FILE__, __LINE__ )

#endif /* !_VLC_THREADS_H */
