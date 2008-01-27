/*****************************************************************************
 * vlc_threads_funcs.h : threads implementation for the VideoLAN client
 * This header provides a portable threads implementation.
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_THREADFUNCS_H_
#define _VLC_THREADFUNCS_H_

/*****************************************************************************
 * Function definitions
 *****************************************************************************/
VLC_EXPORT( void, vlc_threads_error, ( vlc_object_t *) );
VLC_EXPORT( int,  __vlc_mutex_init,    ( vlc_object_t *, vlc_mutex_t * ) );
VLC_EXPORT( int,  __vlc_mutex_init_recursive, ( vlc_object_t *, vlc_mutex_t * ) );
VLC_EXPORT( int,  __vlc_mutex_destroy, ( const char *, int, vlc_mutex_t * ) );
VLC_EXPORT( int,  __vlc_cond_init,     ( vlc_object_t *, vlc_cond_t * ) );
VLC_EXPORT( int,  __vlc_cond_destroy,  ( const char *, int, vlc_cond_t * ) );
VLC_EXPORT( int, __vlc_threadvar_create, (vlc_object_t *, vlc_threadvar_t * ) );
VLC_EXPORT( int,  __vlc_thread_create, ( vlc_object_t *, const char *, int, const char *, void * ( * ) ( void * ), int, vlc_bool_t ) );
VLC_EXPORT( int,  __vlc_thread_set_priority, ( vlc_object_t *, const char *, int, int ) );
VLC_EXPORT( void, __vlc_thread_ready,  ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_thread_join,   ( vlc_object_t *, const char *, int ) );

/*****************************************************************************
 * vlc_threads_error: Signalize an error in the threading system
 *****************************************************************************/
#ifdef NDEBUG
# define vlc_threads_error( P_THIS ) (void)0
#endif

/*****************************************************************************
 * vlc_threads_init: initialize threads system
 *****************************************************************************/
#define vlc_threads_init( P_THIS )                                          \
    __vlc_threads_init( VLC_OBJECT(P_THIS) )

/*****************************************************************************
 * vlc_threads_end: deinitialize threads system
 *****************************************************************************/
#define vlc_threads_end( P_THIS )                                           \
    __vlc_threads_end( VLC_OBJECT(P_THIS) )

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
#define vlc_mutex_init( P_THIS, P_MUTEX )                                   \
    __vlc_mutex_init( VLC_OBJECT(P_THIS), P_MUTEX )

/*****************************************************************************
 * vlc_mutex_init: initialize a recursive mutex (Don't use it)
 *****************************************************************************/
#define vlc_mutex_init_recursive( P_THIS, P_MUTEX )                         \
    __vlc_mutex_init_recursive( VLC_OBJECT(P_THIS), P_MUTEX )

/*****************************************************************************
 * vlc_mutex_lock: lock a mutex
 *****************************************************************************/
#define vlc_mutex_lock( P_MUTEX )                                           \
    __vlc_mutex_lock( __FILE__, __LINE__, P_MUTEX )

#if defined(LIBVLC_USE_PTHREAD)
static inline unsigned long int CAST_PTHREAD_TO_INT (pthread_t th)
{
     union { pthread_t th; unsigned long int i; } v = { };
     v.th = th;
     return v.i;
}
#endif

static inline int __vlc_mutex_lock( const char * psz_file, int i_line,
                                    vlc_mutex_t * p_mutex )
{
    int i_result;
    /* In case of error : */
    unsigned long int i_thread = 0;

#if defined( UNDER_CE )
    EnterCriticalSection( &p_mutex->csection );
    i_result = 0;

#elif defined( WIN32 )
    if( p_mutex->mutex )
    {
        WaitForSingleObject( p_mutex->mutex, INFINITE );
    }
    else
    {
        EnterCriticalSection( &p_mutex->csection );
    }
    i_result = 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( p_mutex == NULL )
    {
        i_result = B_BAD_VALUE;
    }
    else if( p_mutex->init < 2000 )
    {
        i_result = B_NO_INIT;
    }
    else
    {
        i_result = acquire_sem( p_mutex->lock );
    }

#elif defined(LIBVLC_USE_PTHREAD)
#   define vlc_assert_locked( m ) \
           assert (pthread_mutex_lock (&((m)->mutex)) == EDEADLK)

    i_result = pthread_mutex_lock( &p_mutex->mutex );
    if ( i_result )
    {
        i_thread = CAST_PTHREAD_TO_INT(pthread_self());
        errno = i_result;
    }

#endif

    if( i_result )
    {
        msg_Err( p_mutex->p_this,
                 "thread %li: mutex_lock failed at %s:%d (%d:%m)",
                 i_thread, psz_file, i_line, i_result );
        vlc_threads_error( p_mutex->p_this );
    }
    return i_result;
}

#ifndef vlc_assert_locked
# define vlc_assert_locked( m ) (void)0
#endif

/*****************************************************************************
 * vlc_mutex_unlock: unlock a mutex
 *****************************************************************************/
#define vlc_mutex_unlock( P_MUTEX )                                         \
    __vlc_mutex_unlock( __FILE__, __LINE__, P_MUTEX )

static inline int __vlc_mutex_unlock( const char * psz_file, int i_line,
                                      vlc_mutex_t *p_mutex )
{
    int i_result;
    /* In case of error : */
    unsigned long int i_thread = 0;

#if defined( UNDER_CE )
    LeaveCriticalSection( &p_mutex->csection );
    i_result = 0;

#elif defined( WIN32 )
    if( p_mutex->mutex )
    {
        ReleaseMutex( p_mutex->mutex );
    }
    else
    {
        LeaveCriticalSection( &p_mutex->csection );
    }
    i_result = 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( p_mutex == NULL )
    {
        i_result = B_BAD_VALUE;
    }
    else if( p_mutex->init < 2000 )
    {
        i_result = B_NO_INIT;
    }
    else
    {
        release_sem( p_mutex->lock );
        return B_OK;
    }

#elif defined(LIBVLC_USE_PTHREAD)
    i_result = pthread_mutex_unlock( &p_mutex->mutex );
    if ( i_result )
    {
        i_thread = CAST_PTHREAD_TO_INT(pthread_self());
        errno = i_result;
    }

#endif

    if( i_result )
    {
        msg_Err( p_mutex->p_this,
                 "thread %li: mutex_unlock failed at %s:%d (%d:%m)",
                 i_thread, psz_file, i_line, i_result );
        vlc_threads_error( p_mutex->p_this );
    }

    return i_result;
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
    __vlc_cond_init( VLC_OBJECT(P_THIS), P_COND )

/*****************************************************************************
 * vlc_cond_signal: start a thread on condition completion
 *****************************************************************************/
#define vlc_cond_signal( P_COND )                                           \
    __vlc_cond_signal( __FILE__, __LINE__, P_COND )

static inline int __vlc_cond_signal( const char * psz_file, int i_line,
                                     vlc_cond_t *p_condvar )
{
    int i_result;
    /* In case of error : */
    unsigned long int i_thread = 0;

#if defined( UNDER_CE )
    PulseEvent( p_condvar->event );
    i_result = 0;

#elif defined( WIN32 )
    /* Release one waiting thread if one is available. */
    /* For this trick to work properly, the vlc_cond_signal must be surrounded
     * by a mutex. This will prevent another thread from stealing the signal */
    if( !p_condvar->semaphore )
    {
        /*
        ** PulseEvent() only works if none of the waiting threads is suspended.
        ** this is particularily problematic under a debug session. 
        ** as documented in http://support.microsoft.com/kb/q173260/
        */
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
    i_result = 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( p_condvar == NULL )
    {
        i_result = B_BAD_VALUE;
    }
    else if( p_condvar->init < 2000 )
    {
        i_result = B_NO_INIT;
    }
    else
    {
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
        i_result = 0;
    }

#elif defined(LIBVLC_USE_PTHREAD)
    i_result = pthread_cond_signal( &p_condvar->cond );
    if ( i_result )
    {
        i_thread = CAST_PTHREAD_TO_INT(pthread_self());
        errno = i_result;
    }

#endif

    if( i_result )
    {
        msg_Err( p_condvar->p_this,
                 "thread %li: cond_signal failed at %s:%d (%d:%m)",
                 i_thread, psz_file, i_line, i_result );
        vlc_threads_error( p_condvar->p_this );
    }

    return i_result;
}

/*****************************************************************************
 * vlc_cond_wait: wait until condition completion
 *****************************************************************************/
#define vlc_cond_wait( P_COND, P_MUTEX )                                     \
    __vlc_cond_wait( __FILE__, __LINE__, P_COND, P_MUTEX  )

static inline int __vlc_cond_wait( const char * psz_file, int i_line,
                                   vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex )
{
    int i_result;
    /* In case of error : */
    unsigned long int i_thread = 0;

#if defined( UNDER_CE )
    p_condvar->i_waiting_threads++;
    LeaveCriticalSection( &p_mutex->csection );
    WaitForSingleObject( p_condvar->event, INFINITE );
    p_condvar->i_waiting_threads--;

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

    i_result = 0;

#elif defined( WIN32 )
    if( !p_condvar->semaphore )
    {
        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        if( p_mutex->mutex )
        {
            /* It is only possible to atomically release the mutex and
             * initiate the waiting on WinNT/2K/XP. Win9x doesn't have
             * SignalObjectAndWait(). */
            p_condvar->SignalObjectAndWait( p_mutex->mutex,
                                            p_condvar->event,
                                            INFINITE, FALSE );
        }
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

    i_result = 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( p_condvar == NULL )
    {
        i_result = B_BAD_VALUE;
    }
    else if( p_mutex == NULL )
    {
        i_result = B_BAD_VALUE;
    }
    else if( p_condvar->init < 2000 )
    {
        i_result = B_NO_INIT;
    }

    /* The p_condvar->thread var is initialized before the unlock because
     * it enables to identify when the thread is interrupted beetwen the
     * unlock line and the suspend_thread line */
    p_condvar->thread = find_thread( NULL );
    vlc_mutex_unlock( p_mutex );
    suspend_thread( p_condvar->thread );
    p_condvar->thread = -1;

    vlc_mutex_lock( p_mutex );
    i_result = 0;

#elif defined(LIBVLC_USE_PTHREAD)

#   ifdef DEBUG
    /* In debug mode, timeout */
    struct timespec timeout = {
        (mdate() / 1000000) + THREAD_COND_TIMEOUT,
        0 /* 1Hz precision is sufficient here :-) */
    };

    i_result = pthread_cond_timedwait( &p_condvar->cond, &p_mutex->mutex,
                                       &timeout );

    if( i_result == ETIMEDOUT )
    {
        errno = ETIMEDOUT;
        msg_Dbg( p_condvar->p_this,
                  "thread %li: possible condition deadlock "
                  "at %s:%d (%m)", CAST_PTHREAD_TO_INT(pthread_self()),
                  psz_file, i_line );

        i_result = pthread_cond_wait( &p_condvar->cond, &p_mutex->mutex );
    }

#   else
    i_result = pthread_cond_wait( &p_condvar->cond, &p_mutex->mutex );
#   endif

    if ( i_result )
    {
        i_thread = CAST_PTHREAD_TO_INT(pthread_self());
        errno = i_result;
    }

#endif

    if( i_result )
    {
        msg_Err( p_condvar->p_this,
                 "thread %li: cond_wait failed at %s:%d (%d:%m)",
                 i_thread, psz_file, i_line, i_result );
        vlc_threads_error( p_condvar->p_this );
    }

    return i_result;
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
    int i_res;
    unsigned long int i_thread = 0;

#if defined( UNDER_CE )
    mtime_t delay_ms = (deadline - mdate())/1000;

    DWORD result;
    if( delay_ms < 0 )
        delay_ms = 0;

    p_condvar->i_waiting_threads++;
    LeaveCriticalSection( &p_mutex->csection );
    result = WaitForSingleObject( p_condvar->event, delay_ms );
    p_condvar->i_waiting_threads--;

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

    i_res = (int)result;
    if(result == WAIT_TIMEOUT)
       return WAIT_TIMEOUT; /* this error is perfectly normal */

#elif defined( WIN32 )
    DWORD result;

    mtime_t delay_ms = (deadline - mdate())/1000;
    if( delay_ms < 0 )
        delay_ms = 0;

    if( !p_condvar->semaphore )
    {
        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        if( p_mutex->mutex )
        {
            /* It is only possible to atomically release the mutex and
             * initiate the waiting on WinNT/2K/XP. Win9x doesn't have
             * SignalObjectAndWait(). */
            result = p_condvar->SignalObjectAndWait( p_mutex->mutex,
                                            p_condvar->event,
                                            delay_ms, FALSE );
        }
        else
        {
            LeaveCriticalSection( &p_mutex->csection );
            result = WaitForSingleObject( p_condvar->event, delay_ms );
        }

        p_condvar->i_waiting_threads--;
    }
    else if( p_condvar->i_win9x_cv == 1 )
    {
        int i_waiting_threads;

        /* Wait for the gate to be open */
        result = WaitForSingleObject( p_condvar->event, delay_ms );

        /* Increase our wait count */
        p_condvar->i_waiting_threads++;

        LeaveCriticalSection( &p_mutex->csection );
        if( !result )
        {
            /* recaculate remaining delay */
            delay_ms = (deadline - mdate())/1000;
            if( delay_ms < 0 )
                delay_ms = 0;

            result = WaitForSingleObject( p_condvar->semaphore, delay_ms );
        }

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
        result = WaitForSingleObject( p_condvar->semaphore, delay_ms );

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
    if(result == WAIT_TIMEOUT)
       return WAIT_TIMEOUT; /* this error is perfectly normal */

    i_res = (int)result;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
#   error Unimplemented
#elif defined(LIBVLC_USE_PTHREAD)
    lldiv_t d = lldiv( deadline, 1000000 );
    struct timespec ts = { d.quot, d.rem * 1000 };

    i_res = pthread_cond_timedwait( &p_condvar->cond, &p_mutex->mutex, &ts );
    if( i_res == ETIMEDOUT )
        return ETIMEDOUT; /* this error is perfectly normal */
    else
    if ( i_res ) /* other errors = bug */
    {
        i_thread = CAST_PTHREAD_TO_INT(pthread_self());
        errno = i_res;
    }

#endif

    if( i_res )
    {
        msg_Err( p_condvar->p_this,
                 "thread %li: cond_wait failed at %s:%d (%d:%m)",
                 i_thread, psz_file, i_line, i_res );
        vlc_threads_error( p_condvar->p_this );
    }

    return i_res;
}

/*****************************************************************************
 * vlc_cond_destroy: destroy a condition
 *****************************************************************************/
#define vlc_cond_destroy( P_COND )                                          \
    __vlc_cond_destroy( __FILE__, __LINE__, P_COND )

/*****************************************************************************
 * vlc_threadvar_create: create a thread-local variable
 *****************************************************************************/
#define vlc_threadvar_create( PTHIS, P_TLS )                                 \
   __vlc_threadvar_create( PTHIS, P_TLS )

/*****************************************************************************
 * vlc_threadvar_set: create: set the value of a thread-local variable
 *****************************************************************************/
static inline int vlc_threadvar_set( vlc_threadvar_t * p_tls, void *p_value )
{
    int i_ret;

#if defined( HAVE_KERNEL_SCHEDULER_H )
    return -1;

#elif defined( UNDER_CE ) || defined( WIN32 )
    i_ret = ( TlsSetValue( p_tls->handle, p_value ) != 0 );

#elif defined(LIBVLC_USE_PTHREAD)
    i_ret = pthread_setspecific( p_tls->handle, p_value );

#endif

    return i_ret;
}

/*****************************************************************************
 * vlc_threadvar_get: create: get the value of a thread-local variable
 *****************************************************************************/
static inline void* vlc_threadvar_get( vlc_threadvar_t * p_tls )
{
    void* p_ret;

#if defined( HAVE_KERNEL_SCHEDULER_H )
    p_ret = NULL;
#elif defined( UNDER_CE ) || defined( WIN32 )
    p_ret = TlsGetValue( p_tls->handle );

#elif defined(LIBVLC_USE_PTHREAD)
    p_ret = pthread_getspecific( p_tls->handle );

#endif

    return p_ret;
}

# if defined (_POSIX_SPIN_LOCKS) && ((_POSIX_SPIN_LOCKS - 0) > 0)
typedef struct
{
    pthread_spinlock_t  spin;
} vlc_spinlock_t;

/**
 * Initializes a spinlock.
 */
static inline int vlc_spin_init (vlc_spinlock_t *spin)
{
    return pthread_spin_init (&spin->spin, PTHREAD_PROCESS_PRIVATE);
}

/**
 * Acquires a spinlock.
 */
static inline int vlc_spin_lock (vlc_spinlock_t *spin)
{
    return pthread_spin_lock (&spin->spin);
}

/**
 * Releases a spinlock.
 */
static inline int vlc_spin_unlock (vlc_spinlock_t *spin)
{
    return pthread_spin_unlock (&spin->spin);
}

/**
 * Deinitializes a spinlock.
 */
static inline int vlc_spin_destroy (vlc_spinlock_t *spin)
{
    return pthread_spin_destroy (&spin->spin);
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
static inline int vlc_spin_lock (vlc_spinlock_t *spin)
{
    EnterCriticalSection(spin);
    return 0;
}

/**
 * Releases a spinlock.
 */
static inline int vlc_spin_unlock (vlc_spinlock_t *spin)
{
    LeaveCriticalSection(spin);
    return 0;
}

/**
 * Deinitializes a spinlock.
 */
static inline int vlc_spin_destroy (vlc_spinlock_t *spin)
{
    DeleteCriticalSection(spin);
    return 0;
}


#else


/* Fallback to plain mutexes if spinlocks are not available */
typedef vlc_mutex_t vlc_spinlock_t;

static inline int vlc_spin_init (vlc_spinlock_t *spin)
{
    return __vlc_mutex_init (NULL, spin);
}

# define vlc_spin_lock    vlc_mutex_lock
# define vlc_spin_unlock  vlc_mutex_unlock
# define vlc_spin_destroy vlc_mutex_destroy
#endif

/*****************************************************************************
 * vlc_thread_create: create a thread
 *****************************************************************************/
#define vlc_thread_create( P_THIS, PSZ_NAME, FUNC, PRIORITY, WAIT )         \
    __vlc_thread_create( VLC_OBJECT(P_THIS), __FILE__, __LINE__, PSZ_NAME, (void * ( * ) ( void * ))FUNC, PRIORITY, WAIT )

/*****************************************************************************
 * vlc_thread_set_priority: set the priority of the calling thread
 *****************************************************************************/
#define vlc_thread_set_priority( P_THIS, PRIORITY )                         \
    __vlc_thread_set_priority( VLC_OBJECT(P_THIS), __FILE__, __LINE__, PRIORITY )

/*****************************************************************************
 * vlc_thread_ready: tell the parent thread we were successfully spawned
 *****************************************************************************/
#define vlc_thread_ready( P_THIS )                                          \
    __vlc_thread_ready( VLC_OBJECT(P_THIS) )

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits
 *****************************************************************************/
#define vlc_thread_join( P_THIS )                                           \
    __vlc_thread_join( VLC_OBJECT(P_THIS), __FILE__, __LINE__ )

#endif
