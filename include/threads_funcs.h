/*****************************************************************************
 * threads_funcs.h : threads implementation for the VideoLAN client
 * This header provides a portable threads implementation.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: threads_funcs.h,v 1.1 2002/04/27 22:11:22 gbazin Exp $
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

/*****************************************************************************
 * Function definitions
 *****************************************************************************/

/*****************************************************************************
 * vlc_threads_init: initialize threads system
 *****************************************************************************/
static __inline__ int vlc_threads_init( void )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_init();

#elif defined( ST_INIT_IN_ST_H )
    return st_init();

#elif defined( WIN32 )
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return 0;

#elif defined( HAVE_CTHREADS_H )
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    return 0;

#endif
}

/*****************************************************************************
 * vlc_threads_end: stop threads system
 *****************************************************************************/
static __inline__ int vlc_threads_end( void )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_kill();

#elif defined( ST_INIT_IN_ST_H )
    return 0;

#elif defined( WIN32 )
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return 0;

#elif defined( HAVE_CTHREADS_H )
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    return 0;

#endif
}

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_init( vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_mutex_init( p_mutex );

#elif defined( ST_INIT_IN_ST_H )
    *p_mutex = st_mutex_new();
    return ( *p_mutex == NULL ) ? errno : 0;

#elif defined( WIN32 )
    /* We use mutexes on WinNT/2K/XP because we can use the SignalObjectAndWait
     * function and have a 100% correct vlc_cond_wait() implementation.
     * As this function is not available on Win9x, we can use the faster
     * CriticalSections */
    if( (GetVersion() < 0x80000000) && !p_main->p_sys->b_fast_pthread )
    {
        /* We are running on NT/2K/XP, we can use SignalObjectAndWait */
        p_mutex->mutex = CreateMutex( 0, FALSE, 0 );
        return ( p_mutex->mutex ? 0 : 1 );
    }
    else
    {
        InitializeCriticalSection( &p_mutex->csection );
        p_mutex->mutex = NULL;
        return 0;
    }

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
#   if defined(DEBUG) && defined(SYS_LINUX)
    /* Create error-checking mutex to detect threads problems more easily. */
    pthread_mutexattr_t attr;
    int                 i_result;

    pthread_mutexattr_init( &attr );
    pthread_mutexattr_setkind_np( &attr, PTHREAD_MUTEX_ERRORCHECK_NP );
    i_result = pthread_mutex_init( p_mutex, &attr );
    pthread_mutexattr_destroy( &attr );
    return( i_result );
#   endif

    return pthread_mutex_init( p_mutex, NULL );

#elif defined( HAVE_CTHREADS_H )
    mutex_init( p_mutex );
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )

    /* check the arguments and whether it's already been initialized */
    if( p_mutex == NULL )
    {
        return B_BAD_VALUE;
    }

    if( p_mutex->init == 9999 )
    {
        return EALREADY;
    }

    p_mutex->lock = create_sem( 1, "BeMutex" );
    if( p_mutex->lock < B_NO_ERROR )
    {
        return( -1 );
    }

    p_mutex->init = 9999;
    return B_OK;

#endif
}

/*****************************************************************************
 * vlc_mutex_lock: lock a mutex
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_mutex_lock( P_MUTEX )                                        \
        _vlc_mutex_lock( __FILE__, __LINE__, P_MUTEX )
#else
#   define vlc_mutex_lock( P_MUTEX )                                        \
        _vlc_mutex_lock( "(unknown)", 0, P_MUTEX )
#endif

static __inline__ int _vlc_mutex_lock( char * psz_file, int i_line,
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
        intf_ErrMsg( "thread %d error: mutex_lock failed at %s:%d (%s)",
                     pthread_self(), psz_file, i_line, strerror(i_return) );
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
        _vlc_mutex_unlock( __FILE__, __LINE__, P_MUTEX )
#else
#   define vlc_mutex_unlock( P_MUTEX )                                      \
        _vlc_mutex_unlock( "(unknown)", 0, P_MUTEX )
#endif

static __inline__ int _vlc_mutex_unlock( char * psz_file, int i_line,
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
        intf_ErrMsg( "thread %d error: mutex_unlock failed at %s:%d (%s)",
                     pthread_self(), psz_file, i_line, strerror(i_return) );
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
        _vlc_mutex_destroy( __FILE__, __LINE__, P_MUTEX )
#else
#   define vlc_mutex_destroy( P_MUTEX )                                     \
        _vlc_mutex_destroy( "(unknown)", 0, P_MUTEX )
#endif

static __inline__ int _vlc_mutex_destroy( char * psz_file, int i_line,
                                          vlc_mutex_t *p_mutex )
{
#if defined( PTH_INIT_IN_PTH_H )
    return 0;

#elif defined( ST_INIT_IN_ST_H )
    return st_mutex_destroy( *p_mutex );

#elif defined( WIN32 )
    if( p_mutex->mutex )
    {
        CloseHandle( p_mutex->mutex );
    }
    else
    {
        DeleteCriticalSection( &p_mutex->csection );
    }
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )    
    int i_return = pthread_mutex_destroy( p_mutex );
    if( i_return )
    {
        intf_ErrMsg( "thread %d error: mutex_destroy failed at %s:%d (%s)",
                     pthread_self(), psz_file, i_line, strerror(i_return) );
    }
    return i_return;

#elif defined( HAVE_CTHREADS_H )
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( p_mutex->init == 9999 )
    {
        delete_sem( p_mutex->lock );
    }

    p_mutex->init = 0;
    return B_OK;

#endif    
}

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
static __inline__ int vlc_cond_init( vlc_cond_t *p_condvar )
{
#if defined( PTH_INIT_IN_PTH_H )
    return pth_cond_init( p_condvar );

#elif defined( ST_INIT_IN_ST_H )
    *p_condvar = st_cond_new();
    return ( *p_condvar == NULL ) ? errno : 0;

#elif defined( WIN32 )
    /* initialise counter */
    p_condvar->i_waiting_threads = 0;

    /* Create an auto-reset event. */
    p_condvar->signal = CreateEvent( NULL, /* no security */
                                     FALSE,  /* auto-reset event */
                                     FALSE,  /* non-signaled initially */
                                     NULL ); /* unnamed */

    return( !p_condvar->signal );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_cond_init( p_condvar, NULL );

#elif defined( HAVE_CTHREADS_H )
    /* condition_init() */
    spin_lock_init( &p_condvar->lock );
    cthread_queue_init( &p_condvar->queue );
    p_condvar->name = 0;
    p_condvar->implications = 0;

    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( !p_condvar )
    {
        return B_BAD_VALUE;
    }

    if( p_condvar->init == 9999 )
    {
        return EALREADY;
    }

    p_condvar->thread = -1;
    p_condvar->init = 9999;
    return 0;

#endif
}

/*****************************************************************************
 * vlc_cond_signal: start a thread on condition completion
 *****************************************************************************/
static __inline__ int vlc_cond_signal( vlc_cond_t *p_condvar )
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
static __inline__ int vlc_cond_broadcast( vlc_cond_t *p_condvar )
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
#   define vlc_cond_wait( P_COND, P_MUTEX )                                 \
        _vlc_cond_wait( __FILE__, __LINE__, P_COND, P_MUTEX  )
#else
#   define vlc_cond_wait( P_COND, P_MUTEX )                                 \
        _vlc_cond_wait( "(unknown)", 0, P_COND, P_MUTEX )
#endif

static __inline__ int _vlc_cond_wait( char * psz_file, int i_line,
                                      vlc_cond_t *p_condvar,
                                      vlc_mutex_t *p_mutex )
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
        p_main->p_sys->SignalObjectAndWait( p_mutex->mutex, p_condvar->signal,
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

#ifndef DEBUG
    return pthread_cond_wait( p_condvar, p_mutex );
#else
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
            intf_WarnMsg( 1, "thread %d warning: Possible deadlock detected in cond_wait at %s:%d (%s)",
                          pthread_self(), psz_file, i_line, strerror(i_result) );
            continue;
        }

        if( i_result )
        {
            intf_ErrMsg( "thread %d error: cond_wait failed at %s:%d (%s)",
                         pthread_self(), psz_file, i_line, strerror(i_result) );
        }
        return( i_result );
    }
#endif

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
        _vlc_cond_destroy( __FILE__, __LINE__, P_COND )
#else
#   define vlc_cond_destroy( P_COND )                                       \
        _vlc_cond_destroy( "(unknown)", 0, P_COND )
#endif

static __inline__ int _vlc_cond_destroy( char * psz_file, int i_line,
                                         vlc_cond_t *p_condvar )
{
#if defined( PTH_INIT_IN_PTH_H )
    return 0;

#elif defined( ST_INIT_IN_ST_H )
    return st_cond_destroy( *p_condvar );

#elif defined( WIN32 )
    return( !CloseHandle( p_condvar->signal ) );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    int i_result = pthread_cond_destroy( p_condvar );
    if( i_result )
    {
        intf_ErrMsg( "thread %d error: cond_destroy failed at %s:%d (%s)",
                     pthread_self(), psz_file, i_line, strerror(i_result) );
    }
    return i_result;

#elif defined( HAVE_CTHREADS_H )
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    p_condvar->init = 0;
    return 0;

#endif    
}

/*****************************************************************************
 * vlc_thread_create: create a thread
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_thread_create( P_THREAD, PSZ_NAME, FUNC, P_DATA )            \
        _vlc_thread_create( __FILE__, __LINE__, P_THREAD, PSZ_NAME, FUNC, P_DATA )
#else
#   define vlc_thread_create( P_THREAD, PSZ_NAME, FUNC, P_DATA )            \
        _vlc_thread_create( "(unknown)", 0, P_THREAD, PSZ_NAME, FUNC, P_DATA )
#endif

static __inline__ int _vlc_thread_create( char * psz_file, int i_line,
                                          vlc_thread_t *p_thread,
                                          char *psz_name,
                                          vlc_thread_func_t func,
                                          void *p_data )
{
    int i_ret;

#ifdef GPROF
    wrapper_t wrapper;

    /* Initialize the wrapper structure */
    wrapper.func = func;
    wrapper.p_data = p_data;
    getitimer( ITIMER_PROF, &wrapper.itimer );
    vlc_mutex_init( &wrapper.lock );
    vlc_cond_init( &wrapper.wait );
    vlc_mutex_lock( &wrapper.lock );

    /* Alter user-passed data so that we call the wrapper instead
     * of the real function */
    p_data = &wrapper;
    func = vlc_thread_wrapper;
#endif

#if defined( PTH_INIT_IN_PTH_H )
    *p_thread = pth_spawn( PTH_ATTR_DEFAULT, func, p_data );
    i_ret = ( p_thread == NULL );

#elif defined( ST_INIT_IN_ST_H )
    *p_thread = st_thread_create( func, p_data, 1, 0 );
    i_ret = ( p_thread == NULL );
    
#elif defined( WIN32 )
    unsigned threadID;
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with memory
     * leaks and the signal functions not working */
    *p_thread = (HANDLE)_beginthreadex( NULL, 0, (PTHREAD_START) func, 
                                        p_data, 0, &threadID );
    
    i_ret = ( *p_thread ? 0 : 1 );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    i_ret = pthread_create( p_thread, NULL, func, p_data );

#elif defined( HAVE_CTHREADS_H )
    *p_thread = cthread_fork( (cthread_fn_t)func, (any_t)p_data );
    i_ret = 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    *p_thread = spawn_thread( (thread_func)func, psz_name,
                              B_NORMAL_PRIORITY, p_data );
    i_ret = resume_thread( *p_thread );

#endif

#ifdef GPROF
    if( i_ret == 0 )
    {
        vlc_cond_wait( &wrapper.wait, &wrapper.lock );
    }

    vlc_mutex_unlock( &wrapper.lock );
    vlc_mutex_destroy( &wrapper.lock );
    vlc_cond_destroy( &wrapper.wait );
#endif

    if( i_ret == 0 )
    {
        intf_WarnMsg( 2, "thread info: %d (%s) has been created (%s:%d)",
                      *p_thread, psz_name, psz_file, i_line );
    }
    else
    {
        intf_ErrMsg( "thread error: %s couldn't be created at %s:%d (%s)",
                     psz_name, psz_file, i_line, strerror(i_ret) );
    }

    return i_ret;
}

/*****************************************************************************
 * vlc_thread_exit: terminate a thread
 *****************************************************************************/
static __inline__ void vlc_thread_exit( void )
{
#if defined( PTH_INIT_IN_PTH_H )
    pth_exit( 0 );

#elif defined( ST_INIT_IN_ST_H )
    int result;
    st_thread_exit( &result );
    
#elif defined( WIN32 )
    /* For now we don't close the thread handles (because of race conditions).
     * Need to be looked at. */
    _endthreadex(0);

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    pthread_exit( 0 );

#elif defined( HAVE_CTHREADS_H )
    int result;
    cthread_exit( &result );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    exit_thread( 0 );

#endif
}

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits
 *****************************************************************************/
#ifdef DEBUG
#   define vlc_thread_join( THREAD )                                        \
        _vlc_thread_join( __FILE__, __LINE__, THREAD ) 
#else
#   define vlc_thread_join( THREAD )                                        \
        _vlc_thread_join( "(unknown)", 0, THREAD ) 
#endif

static __inline__ void _vlc_thread_join( char * psz_file, int i_line,
                                         vlc_thread_t thread )
{
    int i_ret = 0;

#if defined( PTH_INIT_IN_PTH_H )
    i_ret = pth_join( thread, NULL );

#elif defined( ST_INIT_IN_ST_H )
    i_ret = st_thread_join( thread, NULL );
    
#elif defined( WIN32 )
    WaitForSingleObject( thread, INFINITE );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    i_ret = pthread_join( thread, NULL );

#elif defined( HAVE_CTHREADS_H )
    cthread_join( thread );
    i_ret = 1;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    int32 exit_value;
    wait_for_thread( thread, &exit_value );

#endif

    if( i_ret )
    {
        intf_ErrMsg( "thread error: thread_join(%d) failed at %s:%d (%s)",
                     thread, psz_file, i_line, strerror(i_ret) );
    }
    else
    {
        intf_WarnMsg( 2, "thread info: %d has been joined (%s:%d)",
                      thread, psz_file, i_line );
    }
}

/*****************************************************************************
 * vlc_thread_wrapper: wrapper around thread functions used when profiling.
 *****************************************************************************/
#ifdef GPROF
static void *vlc_thread_wrapper( void *p_wrapper )
{
    /* Put user data in thread-local variables */
    void *            p_data = ((wrapper_t*)p_wrapper)->p_data;
    vlc_thread_func_t func   = ((wrapper_t*)p_wrapper)->func;

    /* Set the profile timer value */
    setitimer( ITIMER_PROF, &((wrapper_t*)p_wrapper)->itimer, NULL );

    /* Tell the calling thread that we don't need its data anymore */
    vlc_mutex_lock( &((wrapper_t*)p_wrapper)->lock );
    vlc_cond_signal( &((wrapper_t*)p_wrapper)->wait );
    vlc_mutex_unlock( &((wrapper_t*)p_wrapper)->lock );

    /* Call the real function */
    return func( p_data );
}
#endif
