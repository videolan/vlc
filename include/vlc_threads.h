/*****************************************************************************
 * threads.h : threads implementation for the VideoLAN client
 * This header provides a portable threads implementation.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vlc_threads.h,v 1.7 2002/07/30 07:56:40 gbazin Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdio.h>

#if defined(GPROF) || defined(DEBUG)
#   include <sys/time.h>
#endif

#if defined( PTH_INIT_IN_PTH_H )                                  /* GNU Pth */
#   include <pth.h>

#elif defined( ST_INIT_IN_ST_H )                            /* State threads */
#   include <st.h>

#elif defined( WIN32 )                                          /* Win32 API */
#   include <process.h>

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )  /* pthreads (like Linux & BSD) */
#   include <pthread.h>
#   ifdef DEBUG
        /* Needed for pthread_cond_timedwait */
#       include <errno.h>
#   endif
    /* This is not prototyped under Linux, though it exists. */
    int pthread_mutexattr_setkind_np( pthread_mutexattr_t *attr, int kind );

#elif defined( HAVE_CTHREADS_H )                                  /* GNUMach */
#   include <cthreads.h>

#elif defined( HAVE_KERNEL_SCHEDULER_H )                             /* BeOS */
#   include <kernel/OS.h>
#   include <kernel/scheduler.h>
#   include <byteorder.h>

#else
#   error no threads available on your system !

#endif

/*****************************************************************************
 * Constants
 *****************************************************************************
 * These constants are used by all threads in *_CreateThread() and
 * *_DestroyThreads() functions. Since those calls are non-blocking, an integer
 * value is used as a shared flag to represent the status of the thread.
 *****************************************************************************/

/* Void status - this value can be used to make sure no operation is currently
 * in progress on the concerned thread in an array of recorded threads */
#define THREAD_NOP          0                            /* nothing happened */

/* Creation status */
#define THREAD_CREATE       10                     /* thread is initializing */
#define THREAD_START        11                          /* thread has forked */
#define THREAD_READY        19                            /* thread is ready */

/* Destructions status */
#define THREAD_DESTROY      20            /* destruction order has been sent */
#define THREAD_END          21        /* destruction order has been received */
#define THREAD_OVER         29             /* thread does not exist any more */

/* Error status */
#define THREAD_ERROR        30                           /* an error occured */
#define THREAD_FATAL        31  /* an fatal error occured - program must end */

/*****************************************************************************
 * Type definitions
 *****************************************************************************/

#if defined( PTH_INIT_IN_PTH_H )
typedef pth_t            vlc_thread_t;
typedef pth_mutex_t      vlc_mutex_t;
typedef pth_cond_t       vlc_cond_t;

#elif defined( ST_INIT_IN_ST_H )
typedef st_thread_t *    vlc_thread_t;
typedef st_mutex_t *     vlc_mutex_t;
typedef st_cond_t *      vlc_cond_t;

#elif defined( WIN32 )
typedef HANDLE vlc_thread_t;
typedef BOOL (WINAPI *SIGNALOBJECTANDWAIT) ( HANDLE, HANDLE, DWORD, BOOL );
typedef unsigned (__stdcall *PTHREAD_START) (void *);

typedef struct
{
    /* WinNT/2K/XP implementation */
    HANDLE              mutex;
    /* Win95/98/ME implementation */
    CRITICAL_SECTION    csection;
} vlc_mutex_t;

typedef struct
{
    volatile int        i_waiting_threads;
    /* WinNT/2K/XP implementation */
    HANDLE              event;
    SIGNALOBJECTANDWAIT SignalObjectAndWait;
    /* Win95/98/ME implementation */
    HANDLE              semaphore;
    CRITICAL_SECTION    csection;
    int                 i_win9x_cv;
} vlc_cond_t;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
typedef pthread_t        vlc_thread_t;
typedef pthread_mutex_t  vlc_mutex_t;
typedef pthread_cond_t   vlc_cond_t;

#elif defined( HAVE_CTHREADS_H )
typedef cthread_t        vlc_thread_t;

/* Those structs are the ones defined in /include/cthreads.h but we need
 * to handle (&foo) where foo is a (mutex_t) while they handle (foo) where
 * foo is a (mutex_t*) */
typedef struct
{
    spin_lock_t held;
    spin_lock_t lock;
    char *name;
    struct cthread_queue queue;
} vlc_mutex_t;

typedef struct
{
    spin_lock_t lock;
    struct cthread_queue queue;
    char *name;
    struct cond_imp *implications;
} vlc_cond_t;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
/* This is the BeOS implementation of the vlc threads, note that the mutex is
 * not a real mutex and the cond_var is not like a pthread cond_var but it is
 * enough for what wee need */

typedef thread_id vlc_thread_t;

typedef struct
{
    int32           init;
    sem_id          lock;
} vlc_mutex_t;

typedef struct
{
    int32           init;
    thread_id       thread;
} vlc_cond_t;

#endif

/*****************************************************************************
 * Function definitions
 *****************************************************************************/
VLC_EXPORT( int,  __vlc_threads_init,  ( vlc_object_t * ) );
VLC_EXPORT( int,  __vlc_threads_end,   ( vlc_object_t * ) );
VLC_EXPORT( int,  __vlc_mutex_init,    ( vlc_object_t *, vlc_mutex_t * ) );
VLC_EXPORT( int,  __vlc_mutex_destroy, ( char *, int, vlc_mutex_t * ) );
VLC_EXPORT( int,  __vlc_cond_init,     ( vlc_object_t *, vlc_cond_t * ) );
VLC_EXPORT( int,  __vlc_cond_destroy,  ( char *, int, vlc_cond_t * ) );
VLC_EXPORT( int,  __vlc_thread_create, ( vlc_object_t *, char *, int, char *, void * ( * ) ( void * ), vlc_bool_t ) );
VLC_EXPORT( void, __vlc_thread_ready,  ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_thread_join,   ( vlc_object_t *, char *, int ) );

/*****************************************************************************
 * vlc_threads_init: initialize threads system
 *****************************************************************************/
#define vlc_threads_init( P_THIS )                                          \
    __vlc_threads_init( CAST_TO_VLC_OBJECT(P_THIS) )

/*****************************************************************************
 * vlc_threads_end: deinitialize threads system
 *****************************************************************************/
#define vlc_threads_end( P_THIS )                                          \
    __vlc_threads_end( CAST_TO_VLC_OBJECT(P_THIS) )

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
#define vlc_mutex_init( P_THIS, P_MUTEX )                                   \
    __vlc_mutex_init( CAST_TO_VLC_OBJECT(P_THIS), P_MUTEX )

/*****************************************************************************
 * vlc_mutex_lock: lock a mutex
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_mutex_lock( P_MUTEX )                                        \
        __vlc_mutex_lock( __FILE__, __LINE__, P_MUTEX )
#else
#   define vlc_mutex_lock( P_MUTEX )                                        \
        __vlc_mutex_lock( "(unknown)", 0, P_MUTEX )
#endif

static inline int __vlc_mutex_lock( char * psz_file, int i_line,
                                    vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_mutex_acquire( p_mutex, TRUE, NULL );

#elif defined( ST_INIT_IN_ST_H )
    return st_mutex_lock( *p_mutex );

#elif defined( WIN32 )
    if( p_mutex->mutex )
    {
        WaitForSingleObject( p_mutex->mutex, INFINITE );
    }
    else
    {
        EnterCriticalSection( &p_mutex->csection );
    }
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    int i_return = pthread_mutex_lock( p_mutex );
    if( i_return )
    {
//        msg_Err( "thread %d: mutex_lock failed at %s:%d (%s)",
//                 pthread_self(), psz_file, i_line, strerror(i_return) );
    }
    return i_return;

#elif defined( HAVE_CTHREADS_H )
    mutex_lock( p_mutex );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    status_t err;

    if( !p_mutex )
    {
        return B_BAD_VALUE;
    }

    if( p_mutex->init < 2000 )
    {
        return B_NO_INIT;
    }

    err = acquire_sem( p_mutex->lock );
    return err;

#endif
}

/*****************************************************************************
 * vlc_mutex_unlock: unlock a mutex
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_mutex_unlock( P_MUTEX )                                      \
        __vlc_mutex_unlock( __FILE__, __LINE__, P_MUTEX )
#else
#   define vlc_mutex_unlock( P_MUTEX )                                      \
        __vlc_mutex_unlock( "(unknown)", 0, P_MUTEX )
#endif

static inline int __vlc_mutex_unlock( char * psz_file, int i_line,
                                      vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_mutex_release( p_mutex );

#elif defined( ST_INIT_IN_ST_H )
    return st_mutex_unlock( *p_mutex );

#elif defined( WIN32 )
    if( p_mutex->mutex )
    {
        ReleaseMutex( p_mutex->mutex );
    }
    else
    {
        LeaveCriticalSection( &p_mutex->csection );
    }
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    int i_return = pthread_mutex_unlock( p_mutex );
    if( i_return )
    {
//        msg_Err( "thread %d: mutex_unlock failed at %s:%d (%s)",
//                 pthread_self(), psz_file, i_line, strerror(i_return) );
    }
    return i_return;

#elif defined( HAVE_CTHREADS_H )
    mutex_unlock( p_mutex );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_mutex)
    {
        return B_BAD_VALUE;
    }

    if( p_mutex->init < 2000 )
    {
        return B_NO_INIT;
    }

    release_sem( p_mutex->lock );
    return B_OK;

#endif
}

/*****************************************************************************
 * vlc_mutex_destroy: destroy a mutex
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_mutex_destroy( P_MUTEX )                                     \
        __vlc_mutex_destroy( __FILE__, __LINE__, P_MUTEX )
#else
#   define vlc_mutex_destroy( P_MUTEX )                                     \
        __vlc_mutex_destroy( "(unknown)", 0, P_MUTEX )
#endif

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
#define vlc_cond_init( P_THIS, P_COND )                                   \
    __vlc_cond_init( CAST_TO_VLC_OBJECT(P_THIS), P_COND )

/*****************************************************************************
 * vlc_cond_signal: start a thread on condition completion
 *****************************************************************************/
static inline int vlc_cond_signal( vlc_cond_t *p_condvar )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_cond_notify( p_condvar, FALSE );

#elif defined( ST_INIT_IN_ST_H )
    return st_cond_signal( *p_condvar );

#elif defined( WIN32 )
    /* Release one waiting thread if one is available. */
    /* For this trick to work properly, the vlc_cond_signal must be surrounded
     * by a mutex. This will prevent another thread from stealing the signal */
    if( !p_condvar->semaphore )
    {
        PulseEvent( p_condvar->event );
    }
    else if( p_condvar->i_win9x_cv == 1 )
    {
        /* Wait for the gate to be open */
        WaitForSingleObject( p_condvar->event, INFINITE );

        if( p_condvar->i_waiting_threads )
        {
            /* Using a semaphore exposes us to a race condition. It is
             * possible for another thread to start waiting on the semaphore
             * just after we signaled it and thus steal the signal.
             * We have to prevent new threads from entering the cond_wait(). */
            ResetEvent( p_condvar->event );

            /* A semaphore is used here because Win9x doesn't have
             * SignalObjectAndWait() and thus a race condition exists
             * during the time we release the mutex and the time we start
             * waiting on the event (more precisely, the signal can sometimes
             * be missed by the waiting thread if we use PulseEvent()). */
            ReleaseSemaphore( p_condvar->semaphore, 1, 0 );
        }
    }
    else
    {
        if( p_condvar->i_waiting_threads )
        {
            ReleaseSemaphore( p_condvar->semaphore, 1, 0 );

            /* Wait for the last thread to be awakened */
            WaitForSingleObject( p_condvar->event, INFINITE );
        }
    }
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_cond_signal( p_condvar );

#elif defined( HAVE_CTHREADS_H )
    /* condition_signal() */
    if ( p_condvar->queue.head || p_condvar->implications )
    {
        cond_signal( (condition_t)p_condvar );
    }
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_condvar )
    {
        return B_BAD_VALUE;
    }

    if( p_condvar->init < 2000 )
    {
        return B_NO_INIT;
    }

    while( p_condvar->thread != -1 )
    {
        thread_info info;
        if( get_thread_info(p_condvar->thread, &info) == B_BAD_VALUE )
        {
            return 0;
        }

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
            return 0;
        }
    }
    return 0;

#endif
}

/*****************************************************************************
 * vlc_cond_broadcast: start all threads waiting on condition completion
 *****************************************************************************/
/*
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * Only works with pthreads, you need to adapt it for others
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 */
static inline int vlc_cond_broadcast( vlc_cond_t *p_condvar )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_cond_notify( p_condvar, FALSE );

#elif defined( ST_INIT_IN_ST_H )
    return st_cond_broadcast( p_condvar );

#elif defined( WIN32 )
    /* Release all waiting threads. */
    int i;

    if( !p_condvar->semaphore )
        for( i = p_condvar->i_waiting_threads; i > 0; i-- )
            PulseEvent( p_condvar->event );
    else if( p_condvar->i_win9x_cv == 1 )
    {
        /* Wait for the gate to be open */
        WaitForSingleObject( p_condvar->event, INFINITE );

        if( p_condvar->i_waiting_threads )
        {
            /* close gate */
            ResetEvent( p_condvar->event );

            ReleaseSemaphore( p_condvar->semaphore,
                              p_condvar->i_waiting_threads, 0 );
        }
    }
    else
    {
        if( p_condvar->i_waiting_threads )
        {
            ReleaseSemaphore( p_condvar->semaphore,
                              p_condvar->i_waiting_threads, 0 );
            /* Wait for the last thread to be awakened */
            WaitForSingleObject( p_condvar->event, INFINITE );
        }
    }

    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_cond_broadcast( p_condvar );

#elif defined( HAVE_CTHREADS_H )
    /* condition_signal() */
    if ( p_condvar->queue.head || p_condvar->implications )
    {
        cond_signal( (condition_t)p_condvar );
    }
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_condvar )
    {
        return B_BAD_VALUE;
    }

    if( p_condvar->init < 2000 )
    {
        return B_NO_INIT;
    }

    while( p_condvar->thread != -1 )
    {
        thread_info info;
        if( get_thread_info(p_condvar->thread, &info) == B_BAD_VALUE )
        {
            return 0;
        }

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
            return 0;
        }
    }
    return 0;

#endif
}

/*****************************************************************************
 * vlc_cond_wait: wait until condition completion
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_cond_wait( P_COND, P_MUTEX )                                   \
        __vlc_cond_wait( __FILE__, __LINE__, P_COND, P_MUTEX  )
#else
#   define vlc_cond_wait( P_COND, P_MUTEX )                                   \
        __vlc_cond_wait( "(unknown)", 0, P_COND, P_MUTEX )
#endif

static inline int __vlc_cond_wait( char * psz_file, int i_line,
                                   vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_cond_await( p_condvar, p_mutex, NULL );

#elif defined( ST_INIT_IN_ST_H )
    int i_ret;

    st_mutex_unlock( *p_mutex );
    i_ret = st_cond_wait( *p_condvar );
    st_mutex_lock( *p_mutex );

    return i_ret;

#elif defined( WIN32 )
    if( !p_condvar->semaphore )
    {
        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        if( p_condvar->SignalObjectAndWait && p_mutex->mutex )
        /* It is only possible to atomically release the mutex and initiate the
         * waiting on WinNT/2K/XP. Win9x doesn't have SignalObjectAndWait(). */
            p_condvar->SignalObjectAndWait( p_mutex->mutex,
                                            p_condvar->event,
                                            INFINITE, FALSE );
        else
        {
            LeaveCriticalSection( &p_mutex->csection );
            WaitForSingleObject( p_condvar->event, INFINITE );
        }

        p_condvar->i_waiting_threads--;
    }
    else if( p_condvar->i_win9x_cv == 1 )
    {
        int i_waiting_threads;

        /* Wait for the gate to be open */
        WaitForSingleObject( p_condvar->event, INFINITE );

        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        LeaveCriticalSection( &p_mutex->csection );
        WaitForSingleObject( p_condvar->semaphore, INFINITE );

        /* Decrement and test must be atomic */
        EnterCriticalSection( &p_condvar->csection );

        /* Decrease our wait count */
        i_waiting_threads = --p_condvar->i_waiting_threads;

        LeaveCriticalSection( &p_condvar->csection );

        /* Reopen the gate if we were the last waiting thread */
        if( !i_waiting_threads )
            SetEvent( p_condvar->event );
    }
    else
    {
        int i_waiting_threads;

        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        LeaveCriticalSection( &p_mutex->csection );
        WaitForSingleObject( p_condvar->semaphore, INFINITE );

        /* Decrement and test must be atomic */
        EnterCriticalSection( &p_condvar->csection );

        /* Decrease our wait count */
        i_waiting_threads = --p_condvar->i_waiting_threads;

        LeaveCriticalSection( &p_condvar->csection );

        /* Signal that the last waiting thread just went through */
        if( !i_waiting_threads )
            SetEvent( p_condvar->event );
    }

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )

#   ifdef DEBUG
    /* In debug mode, timeout */
    struct timeval now;
    struct timespec timeout;
    int    i_result;

    for( ; ; )
    {
        gettimeofday( &now, NULL );
        timeout.tv_sec = now.tv_sec + THREAD_COND_TIMEOUT;
        timeout.tv_nsec = now.tv_usec * 1000;

        i_result = pthread_cond_timedwait( p_condvar, p_mutex, &timeout );

        if( i_result == ETIMEDOUT )
        {
//X            msg_Warn( "thread %d: possible deadlock detected "
//X                      "in cond_wait at %s:%d (%s)", pthread_self(),
//X                      psz_file, i_line, strerror(i_result) );
            continue;
        }

        if( i_result )
        {
//X            msg_Err( "thread %d: cond_wait failed at %s:%d (%s)",
//X                     pthread_self(), psz_file, i_line, strerror(i_result) );
        }
        return( i_result );
    }
#   else
    return pthread_cond_wait( p_condvar, p_mutex );
#   endif

#elif defined( HAVE_CTHREADS_H )
    condition_wait( (condition_t)p_condvar, (mutex_t)p_mutex );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_condvar )
    {
        return B_BAD_VALUE;
    }

    if( !p_mutex )
    {
        return B_BAD_VALUE;
    }

    if( p_condvar->init < 2000 )
    {
        return B_NO_INIT;
    }

    /* The p_condvar->thread var is initialized before the unlock because
     * it enables to identify when the thread is interrupted beetwen the
     * unlock line and the suspend_thread line */
    p_condvar->thread = find_thread( NULL );
    vlc_mutex_unlock( p_mutex );
    suspend_thread( p_condvar->thread );
    p_condvar->thread = -1;

    vlc_mutex_lock( p_mutex );
    return 0;

#endif
}

/*****************************************************************************
 * vlc_cond_destroy: destroy a condition
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_cond_destroy( P_COND )                                       \
        __vlc_cond_destroy( __FILE__, __LINE__, P_COND )
#else
#   define vlc_cond_destroy( P_COND )                                       \
        __vlc_cond_destroy( "(unknown)", 0, P_COND )
#endif

/*****************************************************************************
 * vlc_thread_create: create a thread
 *****************************************************************************/
#   define vlc_thread_create( P_THIS, PSZ_NAME, FUNC, WAIT )                \
        __vlc_thread_create( CAST_TO_VLC_OBJECT(P_THIS), __FILE__, __LINE__, PSZ_NAME, (void * ( * ) ( void * ))FUNC, WAIT )

/*****************************************************************************
 * vlc_thread_ready: tell the parent thread we were successfully spawned
 *****************************************************************************/
#   define vlc_thread_ready( P_THIS )                                       \
        __vlc_thread_ready( CAST_TO_VLC_OBJECT(P_THIS) )

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_thread_join( P_THIS )                                        \
        __vlc_thread_join( CAST_TO_VLC_OBJECT(P_THIS), __FILE__, __LINE__ ) 
#else
#   define vlc_thread_join( P_THIS )                                        \
        __vlc_thread_join( CAST_TO_VLC_OBJECT(P_THIS), "(unknown)", 0 ) 
#endif
