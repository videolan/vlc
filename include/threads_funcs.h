/*****************************************************************************
 * threads_funcs.h : threads implementation for the VideoLAN client
 * This header provides a portable threads implementation.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: threads_funcs.h,v 1.5 2002/06/01 12:31:58 sam Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Function definitions
 *****************************************************************************/
VLC_EXPORT( int,  __vlc_threads_init,  ( vlc_object_t * ) );
VLC_EXPORT( int,    vlc_threads_end,   ( void ) );
VLC_EXPORT( int,  __vlc_mutex_init,    ( vlc_object_t *, vlc_mutex_t * ) );
VLC_EXPORT( int,  __vlc_mutex_destroy, ( char *, int, vlc_mutex_t * ) );
VLC_EXPORT( int,    vlc_cond_init,     ( vlc_cond_t * ) );
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
    PulseEvent( p_condvar->signal );
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
    /* For this trick to work properly, the vlc_cond_signal must be surrounded
     * by a mutex. This will prevent another thread from stealing the signal */
    while( p_condvar->i_waiting_threads )
    {
        PulseEvent( p_condvar->signal );
        Sleep( 1 ); /* deschedule the current thread */
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
    /* The ideal would be to use a function which atomically releases the
     * mutex and initiate the waiting.
     * Unfortunately only the SignalObjectAndWait function does this and it's
     * only supported on WinNT/2K, furthermore it cannot take multiple
     * events as parameters.
     *
     * The solution we use should however fulfill all our needs (even though
     * it is not a correct pthreads implementation)
     */
    int i_result;

    p_condvar->i_waiting_threads ++;

    if( p_mutex->mutex )
    {
        p_mutex->SignalObjectAndWait( p_mutex->mutex, p_condvar->signal,
                                      INFINITE, FALSE );
    }
    else
    {
        /* Release the mutex */
        vlc_mutex_unlock( p_mutex );
        i_result = WaitForSingleObject( p_condvar->signal, INFINITE); 
        p_condvar->i_waiting_threads --;
    }

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

    return( i_result == WAIT_FAILED );

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

