/*****************************************************************************
 * threads.c : threads implementation for the VideoLAN client
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Clément Sténac
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

#include "libvlc.h"
#include <assert.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

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

#if defined( UNDER_CE )
#elif defined( WIN32 )

/* following is only available on NT/2000/XP and above */
static SIGNALOBJECTANDWAIT pf_SignalObjectAndWait = NULL;

/*
** On Windows NT/2K/XP we use a slow mutex implementation but which
** allows us to correctly implement condition variables.
** You can also use the faster Win9x implementation but you might
** experience problems with it.
*/
static vlc_bool_t          b_fast_mutex = VLC_FALSE;
/*
** On Windows 9x/Me you can use a fast but incorrect condition variables
** implementation (more precisely there is a possibility for a race
** condition to happen).
** However it is possible to use slower alternatives which are more robust.
** Currently you can choose between implementation 0 (which is the
** fastest but slightly incorrect), 1 (default) and 2.
*/
static int i_win9x_cv = 1;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( LIBVLC_USE_PTHREAD )
static pthread_mutex_t once_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

vlc_threadvar_t msg_context_global_key;

#ifndef NDEBUG
/*****************************************************************************
 * vlc_threads_error: Report an error from the threading mecanism
 *****************************************************************************
 * This is especially useful to debug those errors, as this is a nice symbol
 * on which you can break.
 *****************************************************************************/
void vlc_threads_error( vlc_object_t *p_this )
{
     msg_Err( p_this, "Error detected. Put a breakpoint in '%s' to debug.",
            __func__ );
}
#endif

/*****************************************************************************
 * vlc_threads_init: initialize threads system
 *****************************************************************************
 * This function requires lazy initialization of a global lock in order to
 * keep the library really thread-safe. Some architectures don't support this
 * and thus do not guarantee the complete reentrancy.
 *****************************************************************************/
int __vlc_threads_init( vlc_object_t *p_this )
{
    libvlc_global_data_t *p_libvlc_global = (libvlc_global_data_t *)p_this;
    int i_ret = VLC_SUCCESS;

    /* If we have lazy mutex initialization, use it. Otherwise, we just
     * hope nothing wrong happens. */
#if defined( UNDER_CE )
#elif defined( WIN32 )
    if( IsDebuggerPresent() )
    {
        /* SignalObjectAndWait() is problematic under a debugger */
        b_fast_mutex = VLC_TRUE;
        i_win9x_cv = 1;
    }
#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( LIBVLC_USE_PTHREAD )
    pthread_mutex_lock( &once_mutex );
#endif

    if( i_status == VLC_THREADS_UNINITIALIZED )
    {
        i_status = VLC_THREADS_PENDING;

        /* We should be safe now. Do all the initialization stuff we want. */
        p_libvlc_global->b_ready = VLC_FALSE;

#if defined( UNDER_CE )
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
                pf_SignalObjectAndWait =
                    (SIGNALOBJECTANDWAIT)GetProcAddress( hInstLib,
                                                     "SignalObjectAndWait" );
            }
        }

#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( LIBVLC_USE_PTHREAD )
#endif

        p_root = vlc_object_create( p_libvlc_global, VLC_OBJECT_GLOBAL );
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

        vlc_threadvar_create( p_root, &msg_context_global_key );
    }
    else
    {
        /* Just increment the initialization count */
        i_initializations++;
    }

    /* If we have lazy mutex initialization support, unlock the mutex;
     * otherwize, do a naive wait loop. */
#if defined( UNDER_CE )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#elif defined( WIN32 )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#elif defined( HAVE_KERNEL_SCHEDULER_H )
    while( i_status == VLC_THREADS_PENDING ) msleep( THREAD_SLEEP );
#elif defined( LIBVLC_USE_PTHREAD )
    pthread_mutex_unlock( &once_mutex );
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
    (void)p_this;
#if defined( UNDER_CE )
#elif defined( WIN32 )
#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( LIBVLC_USE_PTHREAD )
    pthread_mutex_lock( &once_mutex );
#endif

    if( i_initializations == 0 )
        return VLC_EGENERIC;

    i_initializations--;
    if( i_initializations == 0 )
    {
        i_status = VLC_THREADS_UNINITIALIZED;
        vlc_object_destroy( p_root );
    }

#if defined( UNDER_CE )
#elif defined( WIN32 )
#elif defined( HAVE_KERNEL_SCHEDULER_H )
#elif defined( LIBVLC_USE_PTHREAD )
    pthread_mutex_unlock( &once_mutex );
#endif
    return VLC_SUCCESS;
}

#ifdef __linux__
/* This is not prototyped under Linux, though it exists. */
int pthread_mutexattr_setkind_np( pthread_mutexattr_t *attr, int kind );
#endif

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
int __vlc_mutex_init( vlc_object_t *p_this, vlc_mutex_t *p_mutex )
{
    p_mutex->p_this = p_this;

#if defined( UNDER_CE )
    InitializeCriticalSection( &p_mutex->csection );
    return 0;

#elif defined( WIN32 )
    /* We use mutexes on WinNT/2K/XP because we can use the SignalObjectAndWait
     * function and have a 100% correct vlc_cond_wait() implementation.
     * As this function is not available on Win9x, we can use the faster
     * CriticalSections */
    if( pf_SignalObjectAndWait && !b_fast_mutex )
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

#elif defined( LIBVLC_USE_PTHREAD )
# if defined(DEBUG)
    {
        /* Create error-checking mutex to detect problems more easily. */
        pthread_mutexattr_t attr;
        int                 i_result;

        pthread_mutexattr_init( &attr );
#   if defined(SYS_LINUX)
        pthread_mutexattr_setkind_np( &attr, PTHREAD_MUTEX_ERRORCHECK_NP );
#   else
        pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
#   endif

        i_result = pthread_mutex_init( &p_mutex->mutex, &attr );
        pthread_mutexattr_destroy( &attr );
        return( i_result );
    }
# endif
    return pthread_mutex_init( &p_mutex->mutex, NULL );

#endif
}

/*****************************************************************************
 * vlc_mutex_init: initialize a recursive mutex (Do not use)
 *****************************************************************************/
int __vlc_mutex_init_recursive( vlc_object_t *p_this, vlc_mutex_t *p_mutex )
{
#if defined( WIN32 )
    /* Create mutex returns a recursive mutex */
    p_mutex->mutex = CreateMutex( 0, FALSE, 0 );
    return ( p_mutex->mutex != NULL ? 0 : 1 );
#elif defined( LIBVLC_USE_PTHREAD )
    pthread_mutexattr_t attr;
    int                 i_result;

    pthread_mutexattr_init( &attr );
# if defined(DEBUG)
    /* Create error-checking mutex to detect problems more easily. */
#   if defined(SYS_LINUX)
    pthread_mutexattr_setkind_np( &attr, PTHREAD_MUTEX_ERRORCHECK_NP );
#   else
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
#   endif
# endif
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
    i_result = pthread_mutex_init( &p_mutex->mutex, &attr );
    pthread_mutexattr_destroy( &attr );
    return( i_result );
#else
    msg_Err(p_this, "no recursive mutex found. Falling back to regular mutex.\n"
                    "Expect hangs\n")
    return __vlc_mutex_init( p_this, p_mutex );
#endif
}


/*****************************************************************************
 * vlc_mutex_destroy: destroy a mutex, inner version
 *****************************************************************************/
int __vlc_mutex_destroy( const char * psz_file, int i_line, vlc_mutex_t *p_mutex )
{
    int i_result;
    /* In case of error : */
    int i_thread = -1;

#if defined( UNDER_CE )
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

#elif defined( LIBVLC_USE_PTHREAD )
    i_result = pthread_mutex_destroy( &p_mutex->mutex );
    if( i_result )
    {
        i_thread = CAST_PTHREAD_TO_INT(pthread_self());
        errno = i_result;
    }

#endif

    if( i_result )
    {
        msg_Err( p_mutex->p_this,
                 "thread %d: mutex_destroy failed at %s:%d (%d:%m)",
                 i_thread, psz_file, i_line, i_result );
        vlc_threads_error( p_mutex->p_this );
    }
    return i_result;
}

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
int __vlc_cond_init( vlc_object_t *p_this, vlc_cond_t *p_condvar )
{
    p_condvar->p_this = p_this;

#if defined( UNDER_CE )
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
    p_condvar->i_win9x_cv = i_win9x_cv;
    p_condvar->SignalObjectAndWait = pf_SignalObjectAndWait;

    if( (p_condvar->SignalObjectAndWait && !b_fast_mutex)
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

#elif defined( LIBVLC_USE_PTHREAD )
    pthread_condattr_t attr;
    int ret;

    ret = pthread_condattr_init (&attr);
    if (ret)
        return ret;

# if !defined (_POSIX_CLOCK_SELECTION)
   /* Fairly outdated POSIX support (that was defined in 2001) */
#  define _POSIX_CLOCK_SELECTION (-1)
# endif
# if (_POSIX_CLOCK_SELECTION >= 0)
    /* NOTE: This must be the same clock as the one in mtime.c */
    pthread_condattr_setclock (&attr, CLOCK_MONOTONIC);
# endif

    ret = pthread_cond_init (&p_condvar->cond, &attr);
    pthread_condattr_destroy (&attr);
    return ret;

#endif
}

/*****************************************************************************
 * vlc_cond_destroy: destroy a condition, inner version
 *****************************************************************************/
int __vlc_cond_destroy( const char * psz_file, int i_line, vlc_cond_t *p_condvar )
{
    int i_result;
    /* In case of error : */
    int i_thread = -1;

#if defined( UNDER_CE )
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

#elif defined( LIBVLC_USE_PTHREAD )
    i_result = pthread_cond_destroy( &p_condvar->cond );
    if( i_result )
    {
        i_thread = CAST_PTHREAD_TO_INT(pthread_self());
        errno = i_result;
    }

#endif

    if( i_result )
    {
        msg_Err( p_condvar->p_this,
                 "thread %d: cond_destroy failed at %s:%d (%d:%m)",
                 i_thread, psz_file, i_line, i_result );
        vlc_threads_error( p_condvar->p_this );
    }
    return i_result;
}

/*****************************************************************************
 * vlc_tls_create: create a thread-local variable
 *****************************************************************************/
int __vlc_threadvar_create( vlc_object_t *p_this, vlc_threadvar_t *p_tls )
{
    int i_ret = -1;
    (void)p_this;

#if defined( HAVE_KERNEL_SCHEDULER_H )
    msg_Err( p_this, "TLS not implemented" );
    i_ret VLC_EGENERIC;
#elif defined( UNDER_CE ) || defined( WIN32 )
#elif defined( WIN32 )
    p_tls->handle = TlsAlloc();
    i_ret = !( p_tls->handle == 0xFFFFFFFF );

#elif defined( LIBVLC_USE_PTHREAD )
    i_ret =  pthread_key_create( &p_tls->handle, NULL );
#endif
    return i_ret;
}

/*****************************************************************************
 * vlc_thread_create: create a thread, inner version
 *****************************************************************************
 * Note that i_priority is only taken into account on platforms supporting
 * userland real-time priority threads.
 *****************************************************************************/
int __vlc_thread_create( vlc_object_t *p_this, const char * psz_file, int i_line,
                         const char *psz_name, void * ( *func ) ( void * ),
                         int i_priority, vlc_bool_t b_wait )
{
    int i_ret;
    void *p_data = (void *)p_this;
    vlc_object_internals_t *p_priv = p_this->p_internals;

    vlc_mutex_lock( &p_this->object_lock );

#if defined( WIN32 ) || defined( UNDER_CE )
    {
        /* When using the MSVCRT C library you have to use the _beginthreadex
         * function instead of CreateThread, otherwise you'll end up with
         * memory leaks and the signal functions not working (see Microsoft
         * Knowledge Base, article 104641) */
#if defined( UNDER_CE )
        DWORD  threadId;
        HANDLE hThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)func,
                                      (LPVOID)p_data, CREATE_SUSPENDED,
                      &threadId );
#else
        unsigned threadId;
        uintptr_t hThread = _beginthreadex( NULL, 0,
                        (LPTHREAD_START_ROUTINE)func,
                                            (void*)p_data, CREATE_SUSPENDED,
                        &threadId );
#endif
        p_priv->thread_id.id = (DWORD)threadId;
        p_priv->thread_id.hThread = (HANDLE)hThread;
    ResumeThread((HANDLE)hThread);
    }

    i_ret = ( p_priv->thread_id.hThread ? 0 : 1 );

    if( i_ret && i_priority )
    {
        if( !SetThreadPriority(p_priv->thread_id.hThread, i_priority) )
        {
            msg_Warn( p_this, "couldn't set a faster priority" );
            i_priority = 0;
        }
    }

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    p_priv->thread_id = spawn_thread( (thread_func)func, psz_name,
                                      i_priority, p_data );
    i_ret = resume_thread( p_priv->thread_id );

#elif defined( LIBVLC_USE_PTHREAD )
    i_ret = pthread_create( &p_priv->thread_id, NULL, func, p_data );

#ifndef __APPLE__
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
        if( (i_error = pthread_setschedparam( p_priv->thread_id,
                                               i_policy, &param )) )
        {
            errno = i_error;
            msg_Warn( p_this, "couldn't set thread priority (%s:%d): %m",
                      psz_file, i_line );
            i_priority = 0;
        }
    }
#ifndef __APPLE__
    else
    {
        i_priority = 0;
    }
#endif

#endif

    if( i_ret == 0 )
    {
        if( b_wait )
        {
            msg_Dbg( p_this, "waiting for thread completion" );
            vlc_object_wait( p_this );
        }

        p_priv->b_thread = VLC_TRUE;

#if defined( WIN32 ) || defined( UNDER_CE )
        msg_Dbg( p_this, "thread %u (%s) created at priority %d (%s:%d)",
                 (unsigned int)p_priv->thread_id.id, psz_name,
         i_priority, psz_file, i_line );
#else
        msg_Dbg( p_this, "thread %u (%s) created at priority %d (%s:%d)",
                 (unsigned int)p_priv->thread_id, psz_name, i_priority,
                 psz_file, i_line );
#endif


        vlc_mutex_unlock( &p_this->object_lock );
    }
    else
    {
        errno = i_ret;
        msg_Err( p_this, "%s thread could not be created at %s:%d (%m)",
                         psz_name, psz_file, i_line );
        vlc_threads_error( p_this );
        vlc_mutex_unlock( &p_this->object_lock );
    }

    return i_ret;
}

/*****************************************************************************
 * vlc_thread_set_priority: set the priority of the current thread when we
 * couldn't set it in vlc_thread_create (for instance for the main thread)
 *****************************************************************************/
int __vlc_thread_set_priority( vlc_object_t *p_this, const char * psz_file,
                               int i_line, int i_priority )
{
    vlc_object_internals_t *p_priv = p_this->p_internals;
#if defined( WIN32 ) || defined( UNDER_CE )
    if( !p_priv->thread_id.hThread )
        p_priv->thread_id.hThread = GetCurrentThread();
    if( !SetThreadPriority(p_priv->thread_id.hThread, i_priority) )
    {
        msg_Warn( p_this, "couldn't set a faster priority" );
        return 1;
    }

#elif defined( LIBVLC_USE_PTHREAD )
# ifndef __APPLE__
    if( config_GetInt( p_this, "rt-priority" ) > 0 )
# endif
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
        if( !p_priv->thread_id )
            p_priv->thread_id = pthread_self();
        if( (i_error = pthread_setschedparam( p_priv->thread_id,
                                               i_policy, &param )) )
        {
            errno = i_error;
            msg_Warn( p_this, "couldn't set thread priority (%s:%d): %m",
                      psz_file, i_line );
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
    vlc_object_signal( p_this );
}

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits, inner version
 *****************************************************************************/
void __vlc_thread_join( vlc_object_t *p_this, const char * psz_file, int i_line )
{
    vlc_object_internals_t *p_priv = p_this->p_internals;

#if defined( UNDER_CE ) || defined( WIN32 )
    HMODULE hmodule;
    BOOL (WINAPI *OurGetThreadTimes)( HANDLE, FILETIME*, FILETIME*,
                                      FILETIME*, FILETIME* );
    FILETIME create_ft, exit_ft, kernel_ft, user_ft;
    int64_t real_time, kernel_time, user_time;
    HANDLE hThread;
 
    /*
    ** object will close its thread handle when destroyed, duplicate it here
    ** to be on the safe side
    */
    if( ! DuplicateHandle(GetCurrentProcess(),
            p_priv->thread_id.hThread,
            GetCurrentProcess(),
            &hThread,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS) )
    {
        msg_Err( p_this, "thread_join(%u) failed at %s:%d (%s)",
                         (unsigned int)p_priv->thread_id.id,
             psz_file, i_line, GetLastError() );
        vlc_threads_error( p_this );
        p_priv->b_thread = VLC_FALSE;
        return;
    }

    WaitForSingleObject( hThread, INFINITE );

    msg_Dbg( p_this, "thread %u joined (%s:%d)",
             (unsigned int)p_priv->thread_id.id,
             psz_file, i_line );
#if defined( UNDER_CE )
    hmodule = GetModuleHandle( _T("COREDLL") );
#else
    hmodule = GetModuleHandle( _T("KERNEL32") );
#endif
    OurGetThreadTimes = (BOOL (WINAPI*)( HANDLE, FILETIME*, FILETIME*,
                                         FILETIME*, FILETIME* ))
        GetProcAddress( hmodule, _T("GetThreadTimes") );

    if( OurGetThreadTimes &&
        OurGetThreadTimes( hThread,
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
    CloseHandle( hThread );

#else /* !defined(WIN32) */

    int i_ret = 0;

#if defined( HAVE_KERNEL_SCHEDULER_H )
    int32_t exit_value;
    i_ret = (B_OK == wait_for_thread( p_priv->thread_id, &exit_value ));

#elif defined( LIBVLC_USE_PTHREAD )
    i_ret = pthread_join( p_priv->thread_id, NULL );

#endif

    if( i_ret )
    {
        errno = i_ret;
        msg_Err( p_this, "thread_join(%u) failed at %s:%d (%m)",
                         (unsigned int)p_priv->thread_id, psz_file, i_line );
        vlc_threads_error( p_this );
    }
    else
    {
        msg_Dbg( p_this, "thread %u joined (%s:%d)",
                         (unsigned int)p_priv->thread_id, psz_file, i_line );
    }

#endif

    p_priv->b_thread = VLC_FALSE;
}

