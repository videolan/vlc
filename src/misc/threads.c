/*****************************************************************************
 * threads.c : threads implementation for the VideoLAN client
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 VideoLAN
 * $Id: threads.c,v 1.1 2002/06/01 12:32:01 sam Exp $
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

#include <vlc/vlc.h>

#define VLC_THREADS_UNINITIALIZED  0
#define VLC_THREADS_PENDING        1
#define VLC_THREADS_ERROR          2
#define VLC_THREADS_READY          3

/*****************************************************************************
 * Prototype for GPROF wrapper
 *****************************************************************************/
#ifdef GPROF
/* Wrapper function for profiling */
static void *      vlc_thread_wrapper ( void *p_wrapper );

#   ifdef WIN32

#       define ITIMER_REAL 1
#       define ITIMER_PROF 2

struct itimerval
{
    struct timeval it_value;
    struct timeval it_interval;
};  

int setitimer(int kind, const struct itimerval* itnew, struct itimerval* itold);
#   endif /* WIN32 */

typedef struct wrapper_s
{
    /* Data lock access */
    vlc_mutex_t lock;
    vlc_cond_t  wait;
    
    /* Data used to spawn the real thread */
    vlc_thread_func_t func;
    void *p_data;
    
    /* Profiling timer passed to the thread */
    struct itimerval itimer;
    
} wrapper_t;

#endif /* GPROF */

/*****************************************************************************
 * vlc_threads_init: initialize threads system
 *****************************************************************************/
int __vlc_threads_init( vlc_object_t *p_this )
{
    /* FIXME: this is definitely _not_ threadsafe, but at least it works
     * under all implementations. We should for instance use pthread_once
     * for lazy initialization of the global lock. */
    static int i_status = VLC_THREADS_UNINITIALIZED;
    int i_ret;

    if( i_status == VLC_THREADS_READY )
    {
        return 0;
    }

    if( i_status == VLC_THREADS_UNINITIALIZED )
    {
        i_status = VLC_THREADS_PENDING;

#if defined( PTH_INIT_IN_PTH_H )
        i_ret = pth_init();

#elif defined( ST_INIT_IN_ST_H )
        i_ret = st_init();

#elif defined( WIN32 )
        i_ret = 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
        i_ret = 0;

#elif defined( HAVE_CTHREADS_H )
        i_ret = 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
        i_ret = 0;

#endif
        if( i_ret )
        {
            i_status = VLC_THREADS_ERROR;
            return i_ret;
        }

        vlc_mutex_init( p_this, p_this->p_vlc->p_global_lock );

        i_status = VLC_THREADS_READY;

        return i_ret;
    }

    /* Wait until the other thread has initialized the thread library */
    while( i_status == VLC_THREADS_PENDING )
    {
        msleep( THREAD_SLEEP );
    }

    return( i_status == VLC_THREADS_READY );
}

/*****************************************************************************
 * vlc_threads_end: stop threads system
 *****************************************************************************/
int vlc_threads_end( void )
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

static int mutexes = 0;
/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
int __vlc_mutex_init( vlc_object_t *p_this, vlc_mutex_t *p_mutex )
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
    if( (GetVersion() < 0x80000000) && !p_this->p_vlc->p_sys->b_fast_pthread )
    {
        /* We are running on NT/2K/XP, we can use SignalObjectAndWait */
        p_mutex->mutex = CreateMutex( 0, FALSE, 0 );
        p_mutex->SignalObjectAndWait =
                            p_this->p_vlc->p_sys->SignalObjectAndWait;
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
 * vlc_mutex_destroy: destroy a mutex, inner version
 *****************************************************************************/
int __vlc_mutex_destroy( char * psz_file, int i_line, vlc_mutex_t *p_mutex )
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
//X        intf_ErrMsg( "thread %d error: mutex_destroy failed at %s:%d (%s)",
//X                     pthread_self(), psz_file, i_line, strerror(i_return) );
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
int vlc_cond_init( vlc_cond_t *p_condvar )
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
 * vlc_cond_destroy: destroy a condition, inner version
 *****************************************************************************/
int __vlc_cond_destroy( char * psz_file, int i_line, vlc_cond_t *p_condvar )
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
//X        intf_ErrMsg( "thread %d error: cond_destroy failed at %s:%d (%s)",
//X                     pthread_self(), psz_file, i_line, strerror(i_result) );
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
 * vlc_thread_create: create a thread, inner version
 *****************************************************************************/
int __vlc_thread_create( vlc_object_t *p_this, char * psz_file, int i_line,
                         char *psz_name, void * ( *func ) ( void * ),
                         vlc_bool_t b_wait )
{
    int i_ret;

    vlc_mutex_init( p_this, &p_this->thread_lock );
    vlc_cond_init( &p_this->thread_wait );
    vlc_mutex_lock( &p_this->thread_lock );

#ifdef GPROF
    wrapper_t wrapper;

    /* Initialize the wrapper structure */
    wrapper.func = func;
    wrapper.p_data = (void *)p_this;
    getitimer( ITIMER_PROF, &wrapper.itimer );
    vlc_mutex_init( p_this, &wrapper.lock );
    vlc_cond_init( &wrapper.wait );
    vlc_mutex_lock( &wrapper.lock );

    /* Alter user-passed data so that we call the wrapper instead
     * of the real function */
    p_data = &wrapper;
    func = vlc_thread_wrapper;
#endif

#if defined( PTH_INIT_IN_PTH_H )
    p_this->thread_id = pth_spawn( PTH_ATTR_DEFAULT, func, (void *)p_this );
    i_ret = 0;

#elif defined( ST_INIT_IN_ST_H )
    p_this->thread_id = st_thread_create( func, (void *)p_this, 1, 0 );
    i_ret = 0;
    
#elif defined( WIN32 )
    unsigned threadID;
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with memory
     * leaks and the signal functions not working */
    p_this->thread_id = (HANDLE)_beginthreadex( NULL, 0, (PTHREAD_START) func, 
                                                (void *)p_this, 0, &threadID );
    
    i_ret = ( p_this->thread_id ? 0 : 1 );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    i_ret = pthread_create( &p_this->thread_id, NULL, func, (void *)p_this );

#elif defined( HAVE_CTHREADS_H )
    p_this->thread_id = cthread_fork( (cthread_fn_t)func, (any_t)p_this );
    i_ret = 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    p_this->thread_id = spawn_thread( (thread_func)func, psz_name,
                                      B_NORMAL_PRIORITY, (void *)p_this );
    i_ret = resume_thread( p_this->thread_id );

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
        msg_Dbg( p_this, "thread %d (%s) created (%s:%d)",
                         p_this->thread_id, psz_name, psz_file, i_line );

        p_this->b_thread = 1;

        if( b_wait )
        {
            msg_Dbg( p_this, "waiting for thread completion" );
            vlc_cond_wait( &p_this->thread_wait, &p_this->thread_lock );
        }

        vlc_mutex_unlock( &p_this->thread_lock );
    }
    else
    {
        msg_Err( p_this, "%s thread could not be created at %s:%d (%s)",
                         psz_name, psz_file, i_line, strerror(i_ret) );
        vlc_mutex_unlock( &p_this->thread_lock );
        vlc_mutex_destroy( &p_this->thread_lock );
        vlc_cond_destroy( &p_this->thread_wait );
    }

    return i_ret;
}

/*****************************************************************************
 * vlc_thread_ready: tell the parent thread we were successfully spawned
 *****************************************************************************/
void __vlc_thread_ready( vlc_object_t *p_this )
{
    vlc_mutex_lock( &p_this->thread_lock );
    vlc_cond_signal( &p_this->thread_wait );
    vlc_mutex_unlock( &p_this->thread_lock );
}

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits, inner version
 *****************************************************************************/
void __vlc_thread_join( vlc_object_t *p_this, char * psz_file, int i_line )
{
    int i_ret = 0;

#if defined( PTH_INIT_IN_PTH_H )
    i_ret = pth_join( p_this->thread_id, NULL );

#elif defined( ST_INIT_IN_ST_H )
    i_ret = st_thread_join( p_this->thread_id, NULL );
    
#elif defined( WIN32 )
    WaitForSingleObject( p_this->thread_id, INFINITE );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    i_ret = pthread_join( p_this->thread_id, NULL );

#elif defined( HAVE_CTHREADS_H )
    cthread_join( p_this->thread_id );
    i_ret = 1;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    int32 exit_value;
    wait_for_thread( p_this->thread_id, &exit_value );

#endif

    vlc_mutex_destroy( &p_this->thread_lock );
    vlc_cond_destroy( &p_this->thread_wait );

    if( i_ret )
    {
        msg_Err( p_this, "thread_join(%d) failed at %s:%d (%s)",
                         p_this->thread_id, psz_file, i_line, strerror(i_ret) );
    }
    else
    {
        msg_Dbg( p_this, "thread %d joined (%s:%d)",
                         p_this->thread_id, psz_file, i_line );
    }

    p_this->b_thread = 0;
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
