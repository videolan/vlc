/*****************************************************************************
 * threads.c : threads implementation for the VideoLAN client
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
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

#include <stdlib.h>

#define VLC_THREADS_UNINITIALIZED  0
#define VLC_THREADS_PENDING        1
#define VLC_THREADS_ERROR          2
#define VLC_THREADS_READY          3

/*****************************************************************************
 * Global mutex for lazy initialization of the threads system
 *****************************************************************************/
static volatile unsigned i_initializations = 0;
static volatile int i_status = VLC_THREADS_UNINITIALIZED;
static vlc_object_t *p_root;

#if defined( PTH_INIT_IN_PTH_H )
#elif defined( ST_INIT_IN_ST_H )
#elif defined( UNDER_CE )
#elif defined( WIN32 )
#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    static pthread_mutex_t once_mutex = PTHREAD_MUTEX_INITIALIZER;
#elif defined( HAVE_CTHREADS_H )
#endif

/*****************************************************************************
 * Global variable for named mutexes
 *****************************************************************************/
typedef struct vlc_namedmutex_t vlc_namedmutex_t;
struct vlc_namedmutex_t
{
    vlc_mutex_t lock;

    char *psz_name;
    int i_usage;
    vlc_namedmutex_t *p_next;
};

/*****************************************************************************
 * vlc_threads_init: initialize threads system
 *****************************************************************************
 * This function requires lazy initialization of a global lock in order to
 * keep the library really thread-safe. Some architectures don't support this
 * and thus do not guarantee the complete reentrancy.
 *****************************************************************************/
int __vlc_threads_init( vlc_object_t *p_this )
{
    libvlc_t *p_libvlc = (libvlc_t *)p_this;
    int i_ret = VLC_SUCCESS;

    /* If we have lazy mutex initialization, use it. Otherwise, we just
     * hope nothing wrong happens. */
#if defined( PTH_INIT_IN_PTH_H )
#elif defined( ST_INIT_IN_ST_H )
#elif defined( UNDER_CE )
#elif defined( WIN32 )
#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    pthread_mutex_lock( &once_mutex );
#elif defined( HAVE_CTHREADS_H )
#endif

    if( i_status == VLC_THREADS_UNINITIALIZED )
    {
        i_status = VLC_THREADS_PENDING;

        /* We should be safe now. Do all the initialization stuff we want. */
        p_libvlc->b_ready = VLC_FALSE;

#if defined( PTH_INIT_IN_PTH_H )
        i_ret = ( pth_init() == FALSE );

#elif defined( ST_INIT_IN_ST_H )
        i_ret = st_init();

#elif defined( UNDER_CE )
        /* Nothing to initialize */

#elif defined( WIN32 )
        /* Dynamically get the address of SignalObjectAndWait */
        if( GetVersion() < 0x80000000 )
        {
            HINSTANCE hInstLib;

            /* We are running on NT/2K/XP, we can use SignalObjectAndWait */
            hInstLib = LoadLibrary( "kernel32" );
            if( hInstLib )
            {
                p_libvlc->SignalObjectAndWait =
                    (SIGNALOBJECTANDWAIT)GetProcAddress( hInstLib,
                                                     "SignalObjectAndWait" );
            }
        }
        else
        {
            p_libvlc->SignalObjectAndWait = NULL;
        }

        p_libvlc->b_fast_mutex = 0;
        p_libvlc->i_win9x_cv = 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
#elif defined( HAVE_CTHREADS_H )
#endif

        p_root = vlc_object_create( p_libvlc, VLC_OBJECT_ROOT );
        if( p_root == NULL )
            i_ret = VLC_ENOMEM;

        if( i_ret )
        {
            i_status = VLC_THREADS_ERROR;
        }
        else
        {
            i_initializations++;
            i_status = VLC_THREADS_READY;
        }
    }
    else
    {
        /* Just increment the initialization count */
        i_initializations++;
    }

    /* If we have lazy mutex initialization support, unlock the mutex;
     * otherwize, do a naive wait loop. */
#if defined( PTH_INIT_IN_PTH_H )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#elif defined( ST_INIT_IN_ST_H )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#elif defined( UNDER_CE )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#elif defined( WIN32 )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#elif defined( HAVE_KERNEL_SCHEDULER_H )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    pthread_mutex_unlock( &once_mutex );
#elif defined( HAVE_CTHREADS_H )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#endif

    if( i_status != VLC_THREADS_READY )
    {
        return VLC_ETHREAD;
    }

    return i_ret;
}

/*****************************************************************************
 * vlc_threads_end: stop threads system
 *****************************************************************************
 * FIXME: This function is far from being threadsafe.
 *****************************************************************************/
int __vlc_threads_end( vlc_object_t *p_this )
{
#if defined( PTH_INIT_IN_PTH_H )
#elif defined( ST_INIT_IN_ST_H )
#elif defined( UNDER_CE )
#elif defined( WIN32 )
#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    pthread_mutex_lock( &once_mutex );
#elif defined( HAVE_CTHREADS_H )
#endif

    if( i_initializations == 0 )
        return VLC_EGENERIC;

    i_initializations--;
    if( i_initializations == 0 )
    {
        i_status = VLC_THREADS_UNINITIALIZED;
        vlc_object_destroy( p_root );
    }

#if defined( PTH_INIT_IN_PTH_H )
    if( i_initializations == 0 )
    {
        return ( pth_kill() == FALSE );
    }

#elif defined( ST_INIT_IN_ST_H )
#elif defined( UNDER_CE )
#elif defined( WIN32 )
#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    pthread_mutex_unlock( &once_mutex );
#elif defined( HAVE_CTHREADS_H )
#endif
    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
int __vlc_mutex_init( vlc_object_t *p_this, vlc_mutex_t *p_mutex )
{
    p_mutex->p_this = p_this;

#if defined( PTH_INIT_IN_PTH_H )
    return ( pth_mutex_init( &p_mutex->mutex ) == FALSE );

#elif defined( ST_INIT_IN_ST_H )
    p_mutex->mutex = st_mutex_new();
    return ( p_mutex->mutex == NULL ) ? errno : 0;

#elif defined( UNDER_CE )
    InitializeCriticalSection( &p_mutex->csection );
    return 0;

#elif defined( WIN32 )
    /* We use mutexes on WinNT/2K/XP because we can use the SignalObjectAndWait
     * function and have a 100% correct vlc_cond_wait() implementation.
     * As this function is not available on Win9x, we can use the faster
     * CriticalSections */
    if( p_this->p_libvlc->SignalObjectAndWait &&
        !p_this->p_libvlc->b_fast_mutex )
    {
        /* We are running on NT/2K/XP, we can use SignalObjectAndWait */
        p_mutex->mutex = CreateMutex( 0, FALSE, 0 );
        return ( p_mutex->mutex != NULL ? 0 : 1 );
    }
    else
    {
        p_mutex->mutex = NULL;
        InitializeCriticalSection( &p_mutex->csection );
        return 0;
    }

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

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
#   if defined(DEBUG) && defined(SYS_LINUX)
    {
        /* Create error-checking mutex to detect problems more easily. */
        pthread_mutexattr_t attr;
        int                 i_result;

        pthread_mutexattr_init( &attr );
        pthread_mutexattr_setkind_np( &attr, PTHREAD_MUTEX_ERRORCHECK_NP );
        i_result = pthread_mutex_init( &p_mutex->mutex, &attr );
        pthread_mutexattr_destroy( &attr );
        return( i_result );
    }
#   endif
    return pthread_mutex_init( &p_mutex->mutex, NULL );

#elif defined( HAVE_CTHREADS_H )
    mutex_init( p_mutex );
    return 0;

#endif
}

/*****************************************************************************
 * vlc_mutex_destroy: destroy a mutex, inner version
 *****************************************************************************/
int __vlc_mutex_destroy( char * psz_file, int i_line, vlc_mutex_t *p_mutex )
{
    int i_result;
    /* In case of error : */
    int i_thread = -1;
    const char * psz_error = "";

#if defined( PTH_INIT_IN_PTH_H )
    return 0;

#elif defined( ST_INIT_IN_ST_H )
    i_result = st_mutex_destroy( p_mutex->mutex );

#elif defined( UNDER_CE )
    DeleteCriticalSection( &p_mutex->csection );
    return 0;

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

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( p_mutex->init == 9999 )
    {
        delete_sem( p_mutex->lock );
    }

    p_mutex->init = 0;
    return B_OK;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    i_result = pthread_mutex_destroy( &p_mutex->mutex );
    if( i_result )
    {
        i_thread = (int)pthread_self();
        psz_error = strerror(i_result);
    }

#elif defined( HAVE_CTHREADS_H )
    return 0;

#endif

    if( i_result )
    {
        msg_Err( p_mutex->p_this,
                 "thread %d: mutex_destroy failed at %s:%d (%d:%s)",
                 i_thread, psz_file, i_line, i_result, psz_error );
    }
    return i_result;
}

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
int __vlc_cond_init( vlc_object_t *p_this, vlc_cond_t *p_condvar )
{
    p_condvar->p_this = p_this;

#if defined( PTH_INIT_IN_PTH_H )
    return ( pth_cond_init( &p_condvar->cond ) == FALSE );

#elif defined( ST_INIT_IN_ST_H )
    p_condvar->cond = st_cond_new();
    return ( p_condvar->cond == NULL ) ? errno : 0;

#elif defined( UNDER_CE )
    /* Initialize counter */
    p_condvar->i_waiting_threads = 0;

    /* Create an auto-reset event. */
    p_condvar->event = CreateEvent( NULL,   /* no security */
                                    FALSE,  /* auto-reset event */
                                    FALSE,  /* start non-signaled */
                                    NULL ); /* unnamed */
    return !p_condvar->event;

#elif defined( WIN32 )
    /* Initialize counter */
    p_condvar->i_waiting_threads = 0;

    /* Misc init */
    p_condvar->i_win9x_cv = p_this->p_libvlc->i_win9x_cv;
    p_condvar->SignalObjectAndWait = p_this->p_libvlc->SignalObjectAndWait;

    if( (p_condvar->SignalObjectAndWait && !p_this->p_libvlc->b_fast_mutex)
        || p_condvar->i_win9x_cv == 0 )
    {
        /* Create an auto-reset event. */
        p_condvar->event = CreateEvent( NULL,   /* no security */
                                        FALSE,  /* auto-reset event */
                                        FALSE,  /* start non-signaled */
                                        NULL ); /* unnamed */

        p_condvar->semaphore = NULL;
        return !p_condvar->event;
    }
    else
    {
        p_condvar->semaphore = CreateSemaphore( NULL,       /* no security */
                                                0,          /* initial count */
                                                0x7fffffff, /* max count */
                                                NULL );     /* unnamed */

        if( p_condvar->i_win9x_cv == 1 )
            /* Create a manual-reset event initially signaled. */
            p_condvar->event = CreateEvent( NULL, TRUE, TRUE, NULL );
        else
            /* Create a auto-reset event. */
            p_condvar->event = CreateEvent( NULL, FALSE, FALSE, NULL );

        InitializeCriticalSection( &p_condvar->csection );

        return !p_condvar->semaphore || !p_condvar->event;
    }

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

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    return pthread_cond_init( &p_condvar->cond, NULL );

#elif defined( HAVE_CTHREADS_H )
    /* condition_init() */
    spin_lock_init( &p_condvar->lock );
    cthread_queue_init( &p_condvar->queue );
    p_condvar->name = 0;
    p_condvar->implications = 0;

    return 0;

#endif
}

/*****************************************************************************
 * vlc_cond_destroy: destroy a condition, inner version
 *****************************************************************************/
int __vlc_cond_destroy( char * psz_file, int i_line, vlc_cond_t *p_condvar )
{
    int i_result;
    /* In case of error : */
    int i_thread = -1;
    const char * psz_error = "";

#if defined( PTH_INIT_IN_PTH_H )
    return 0;

#elif defined( ST_INIT_IN_ST_H )
    i_result = st_cond_destroy( p_condvar->cond );

#elif defined( UNDER_CE )
    i_result = !CloseHandle( p_condvar->event );

#elif defined( WIN32 )
    if( !p_condvar->semaphore )
        i_result = !CloseHandle( p_condvar->event );
    else
        i_result = !CloseHandle( p_condvar->event )
          || !CloseHandle( p_condvar->semaphore );

    if( p_condvar->semaphore != NULL )
		DeleteCriticalSection( &p_condvar->csection );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    p_condvar->init = 0;
    return 0;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    i_result = pthread_cond_destroy( &p_condvar->cond );
    if( i_result )
    {
        i_thread = (int)pthread_self();
        psz_error = strerror(i_result);
    }

#elif defined( HAVE_CTHREADS_H )
    return 0;

#endif

    if( i_result )
    {
        msg_Err( p_condvar->p_this,
                 "thread %d: cond_destroy failed at %s:%d (%d:%s)",
                 i_thread, psz_file, i_line, i_result, psz_error );
    }
    return i_result;
}

/*****************************************************************************
 * vlc_thread_create: create a thread, inner version
 *****************************************************************************
 * Note that i_priority is only taken into account on platforms supporting
 * userland real-time priority threads.
 *****************************************************************************/
int __vlc_thread_create( vlc_object_t *p_this, char * psz_file, int i_line,
                         char *psz_name, void * ( *func ) ( void * ),
                         int i_priority, vlc_bool_t b_wait )
{
    int i_ret;
    void *p_data = (void *)p_this;

    vlc_mutex_lock( &p_this->object_lock );

#if defined( PTH_INIT_IN_PTH_H )
    p_this->thread_id = pth_spawn( PTH_ATTR_DEFAULT, func, p_data );
    i_ret = p_this->thread_id == NULL;

#elif defined( ST_INIT_IN_ST_H )
    p_this->thread_id = st_thread_create( func, p_data, 1, 0 );
    i_ret = 0;

#elif defined( WIN32 ) || defined( UNDER_CE )
    {
        unsigned threadID;
        /* When using the MSVCRT C library you have to use the _beginthreadex
         * function instead of CreateThread, otherwise you'll end up with
         * memory leaks and the signal functions not working (see Microsoft
         * Knowledge Base, article 104641) */
        p_this->thread_id =
#if defined( UNDER_CE )
                (HANDLE)CreateThread( NULL, 0, (PTHREAD_START) func,
                                      p_data, 0, &threadID );
#else
                (HANDLE)_beginthreadex( NULL, 0, (PTHREAD_START) func,
                                        p_data, 0, &threadID );
#endif
    }

    if( p_this->thread_id && i_priority )
    {
        if( !SetThreadPriority(p_this->thread_id, i_priority) )
        {
            msg_Warn( p_this, "couldn't set a faster priority" );
            i_priority = 0;
        }
    }

    i_ret = ( p_this->thread_id ? 0 : 1 );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    p_this->thread_id = spawn_thread( (thread_func)func, psz_name,
                                      i_priority, p_data );
    i_ret = resume_thread( p_this->thread_id );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    i_ret = pthread_create( &p_this->thread_id, NULL, func, p_data );

#ifndef SYS_DARWIN
    if( config_GetInt( p_this, "rt-priority" ) )
#endif
    {
        int i_error, i_policy;
        struct sched_param param;

        memset( &param, 0, sizeof(struct sched_param) );
        if( config_GetType( p_this, "rt-offset" ) )
        {
            i_priority += config_GetInt( p_this, "rt-offset" );
        }
        if( i_priority <= 0 )
        {
            param.sched_priority = (-1) * i_priority;
            i_policy = SCHED_OTHER;
        }
        else
        {
            param.sched_priority = i_priority;
            i_policy = SCHED_RR;
        }
        if( (i_error = pthread_setschedparam( p_this->thread_id,
                                               i_policy, &param )) )
        {
            msg_Warn( p_this, "couldn't set thread priority (%s:%d): %s",
                      psz_file, i_line, strerror(i_error) );
            i_priority = 0;
        }
    }
#ifndef SYS_DARWIN
    else
    {
        i_priority = 0;
    }
#endif

#elif defined( HAVE_CTHREADS_H )
    p_this->thread_id = cthread_fork( (cthread_fn_t)func, (any_t)p_data );
    i_ret = 0;

#endif

    if( i_ret == 0 )
    {
        if( b_wait )
        {
            msg_Dbg( p_this, "waiting for thread completion" );
            vlc_cond_wait( &p_this->object_wait, &p_this->object_lock );
        }

        p_this->b_thread = 1;

        msg_Dbg( p_this, "thread %u (%s) created at priority %d (%s:%d)",
                 (unsigned int)p_this->thread_id, psz_name, i_priority,
                 psz_file, i_line );

        vlc_mutex_unlock( &p_this->object_lock );
    }
    else
    {
#ifdef HAVE_STRERROR
        msg_Err( p_this, "%s thread could not be created at %s:%d (%s)",
                         psz_name, psz_file, i_line, strerror(i_ret) );
#else
        msg_Err( p_this, "%s thread could not be created at %s:%d",
                         psz_name, psz_file, i_line );
#endif
        vlc_mutex_unlock( &p_this->object_lock );
    }

    return i_ret;
}

/*****************************************************************************
 * vlc_thread_set_priority: set the priority of the current thread when we
 * couldn't set it in vlc_thread_create (for instance for the main thread)
 *****************************************************************************/
int __vlc_thread_set_priority( vlc_object_t *p_this, char * psz_file,
                               int i_line, int i_priority )
{
#if defined( PTH_INIT_IN_PTH_H ) || defined( ST_INIT_IN_ST_H )
#elif defined( WIN32 ) || defined( UNDER_CE )
    if( !SetThreadPriority(GetCurrentThread(), i_priority) )
    {
        msg_Warn( p_this, "couldn't set a faster priority" );
        return 1;
    }

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
#ifndef SYS_DARWIN
    if( config_GetInt( p_this, "rt-priority" ) )
#endif
    {
        int i_error, i_policy;
        struct sched_param param;

        memset( &param, 0, sizeof(struct sched_param) );
        if( config_GetType( p_this, "rt-offset" ) )
        {
            i_priority += config_GetInt( p_this, "rt-offset" );
        }
        if( i_priority <= 0 )
        {
            param.sched_priority = (-1) * i_priority;
            i_policy = SCHED_OTHER;
        }
        else
        {
            param.sched_priority = i_priority;
            i_policy = SCHED_RR;
        }
        if( !p_this->thread_id )
            p_this->thread_id = pthread_self();
        if( (i_error = pthread_setschedparam( p_this->thread_id,
                                               i_policy, &param )) )
        {
            msg_Warn( p_this, "couldn't set thread priority (%s:%d): %s",
                      psz_file, i_line, strerror(i_error) );
            i_priority = 0;
        }
    }
#endif

    return 0;
}

/*****************************************************************************
 * vlc_thread_ready: tell the parent thread we were successfully spawned
 *****************************************************************************/
void __vlc_thread_ready( vlc_object_t *p_this )
{
    vlc_mutex_lock( &p_this->object_lock );
    vlc_cond_signal( &p_this->object_wait );
    vlc_mutex_unlock( &p_this->object_lock );
}

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits, inner version
 *****************************************************************************/
void __vlc_thread_join( vlc_object_t *p_this, char * psz_file, int i_line )
{
    int i_ret = 0;

#if defined( PTH_INIT_IN_PTH_H )
    i_ret = ( pth_join( p_this->thread_id, NULL ) == FALSE );

#elif defined( ST_INIT_IN_ST_H )
    i_ret = st_thread_join( p_this->thread_id, NULL );

#elif defined( UNDER_CE ) || defined( WIN32 )
    HMODULE hmodule;
    BOOL (WINAPI *OurGetThreadTimes)( HANDLE, FILETIME*, FILETIME*,
                                      FILETIME*, FILETIME* );
    FILETIME create_ft, exit_ft, kernel_ft, user_ft;
    int64_t real_time, kernel_time, user_time;

    WaitForSingleObject( p_this->thread_id, INFINITE );

#if defined( UNDER_CE )
    hmodule = GetModuleHandle( _T("COREDLL") );
#else
    hmodule = GetModuleHandle( _T("KERNEL32") );
#endif
    OurGetThreadTimes = (BOOL (WINAPI*)( HANDLE, FILETIME*, FILETIME*,
                                         FILETIME*, FILETIME* ))
        GetProcAddress( hmodule, _T("GetThreadTimes") );

    if( OurGetThreadTimes &&
        OurGetThreadTimes( p_this->thread_id,
                           &create_ft, &exit_ft, &kernel_ft, &user_ft ) )
    {
        real_time =
          ((((int64_t)exit_ft.dwHighDateTime)<<32)| exit_ft.dwLowDateTime) -
          ((((int64_t)create_ft.dwHighDateTime)<<32)| create_ft.dwLowDateTime);
        real_time /= 10;

        kernel_time =
          ((((int64_t)kernel_ft.dwHighDateTime)<<32)|
           kernel_ft.dwLowDateTime) / 10;

        user_time =
          ((((int64_t)user_ft.dwHighDateTime)<<32)|
           user_ft.dwLowDateTime) / 10;

        msg_Dbg( p_this, "thread times: "
                 "real "I64Fd"m%fs, kernel "I64Fd"m%fs, user "I64Fd"m%fs",
                 real_time/60/1000000,
                 (double)((real_time%(60*1000000))/1000000.0),
                 kernel_time/60/1000000,
                 (double)((kernel_time%(60*1000000))/1000000.0),
                 user_time/60/1000000,
                 (double)((user_time%(60*1000000))/1000000.0) );
    }
    CloseHandle( p_this->thread_id );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    int32_t exit_value;
    wait_for_thread( p_this->thread_id, &exit_value );

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
    i_ret = pthread_join( p_this->thread_id, NULL );

#elif defined( HAVE_CTHREADS_H )
    cthread_join( p_this->thread_id );
    i_ret = 1;

#endif

    if( i_ret )
    {
#ifdef HAVE_STRERROR
        msg_Err( p_this, "thread_join(%u) failed at %s:%d (%s)",
                         (unsigned int)p_this->thread_id, psz_file, i_line,
                         strerror(i_ret) );
#else
        msg_Err( p_this, "thread_join(%u) failed at %s:%d",
                         (unsigned int)p_this->thread_id, psz_file, i_line );
#endif
    }
    else
    {
        msg_Dbg( p_this, "thread %u joined (%s:%d)",
                         (unsigned int)p_this->thread_id, psz_file, i_line );
    }

    p_this->b_thread = 0;
}

