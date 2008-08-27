/*****************************************************************************
 * threads.c : threads implementation for the VideoLAN client
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
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

#include <vlc_common.h>

#include "libvlc.h"
#include <stdarg.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <signal.h>

#define VLC_THREADS_UNINITIALIZED  0
#define VLC_THREADS_PENDING        1
#define VLC_THREADS_ERROR          2
#define VLC_THREADS_READY          3

/*****************************************************************************
 * Global mutex for lazy initialization of the threads system
 *****************************************************************************/
static volatile unsigned i_initializations = 0;

#if defined( LIBVLC_USE_PTHREAD )
# include <sched.h>

static pthread_mutex_t once_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/**
 * Global process-wide VLC object.
 * Contains inter-instance data, such as the module cache and global mutexes.
 */
static libvlc_global_data_t *p_root;

libvlc_global_data_t *vlc_global( void )
{
    assert( i_initializations > 0 );
    return p_root;
}

#ifndef NDEBUG
/**
 * Object running the current thread
 */
static vlc_threadvar_t thread_object_key;

vlc_object_t *vlc_threadobj (void)
{
    return vlc_threadvar_get (&thread_object_key);
}
#endif

vlc_threadvar_t msg_context_global_key;

#if defined(LIBVLC_USE_PTHREAD)
static inline unsigned long vlc_threadid (void)
{
     union { pthread_t th; unsigned long int i; } v = { };
     v.th = pthread_self ();
     return v.i;
}

#if defined(HAVE_EXECINFO_H) && defined(HAVE_BACKTRACE)
# include <execinfo.h>
#endif

/*****************************************************************************
 * vlc_thread_fatal: Report an error from the threading layer
 *****************************************************************************
 * This is mostly meant for debugging.
 *****************************************************************************/
void vlc_pthread_fatal (const char *action, int error,
                        const char *file, unsigned line)
{
    fprintf (stderr, "LibVLC fatal error %s in thread %lu at %s:%u: %d\n",
             action, vlc_threadid (), file, line, error);

    /* Sometimes strerror_r() crashes too, so make sure we print an error
     * message before we invoke it */
#ifdef __GLIBC__
    /* Avoid the strerror_r() prototype brain damage in glibc */
    errno = error;
    fprintf (stderr, " Error message: %m at:\n");
#else
    char buf[1000];
    const char *msg;

    switch (strerror_r (error, buf, sizeof (buf)))
    {
        case 0:
            msg = buf;
            break;
        case ERANGE: /* should never happen */
            msg = "unknwon (too big to display)";
            break;
        default:
            msg = "unknown (invalid error number)";
            break;
    }
    fprintf (stderr, " Error message: %s\n", msg);
#endif
    fflush (stderr);

#ifdef HAVE_BACKTRACE
    void *stack[20];
    int len = backtrace (stack, sizeof (stack) / sizeof (stack[0]));
    backtrace_symbols_fd (stack, len, 2);
#endif

    abort ();
}
#else
void vlc_pthread_fatal (const char *action, int error,
                        const char *file, unsigned line)
{
    (void)action; (void)error; (void)file; (void)line;
    abort();
}

static vlc_threadvar_t cancel_key;
#endif

/*****************************************************************************
 * vlc_threads_init: initialize threads system
 *****************************************************************************
 * This function requires lazy initialization of a global lock in order to
 * keep the library really thread-safe. Some architectures don't support this
 * and thus do not guarantee the complete reentrancy.
 *****************************************************************************/
int vlc_threads_init( void )
{
    int i_ret = VLC_SUCCESS;

    /* If we have lazy mutex initialization, use it. Otherwise, we just
     * hope nothing wrong happens. */
#if defined( LIBVLC_USE_PTHREAD )
    pthread_mutex_lock( &once_mutex );
#endif

    if( i_initializations == 0 )
    {
        p_root = vlc_custom_create( (vlc_object_t *)NULL, sizeof( *p_root ),
                                    VLC_OBJECT_GENERIC, "root" );
        if( p_root == NULL )
        {
            i_ret = VLC_ENOMEM;
            goto out;
        }

        /* We should be safe now. Do all the initialization stuff we want. */
#ifndef NDEBUG
        vlc_threadvar_create( &thread_object_key, NULL );
#endif
        vlc_threadvar_create( &msg_context_global_key, msg_StackDestroy );
#ifndef LIBVLC_USE_PTHREAD
        vlc_threadvar_create( &cancel_key, free );
#endif
    }
    i_initializations++;

out:
    /* If we have lazy mutex initialization support, unlock the mutex.
     * Otherwize, we are screwed. */
#if defined( LIBVLC_USE_PTHREAD )
    pthread_mutex_unlock( &once_mutex );
#endif

    return i_ret;
}

/*****************************************************************************
 * vlc_threads_end: stop threads system
 *****************************************************************************
 * FIXME: This function is far from being threadsafe.
 *****************************************************************************/
void vlc_threads_end( void )
{
#if defined( LIBVLC_USE_PTHREAD )
    pthread_mutex_lock( &once_mutex );
#endif

    assert( i_initializations > 0 );

    if( i_initializations == 1 )
    {
        vlc_object_release( p_root );
#ifndef LIBVLC_USE_PTHREAD
        vlc_threadvar_delete( &cancel_key );
#endif
        vlc_threadvar_delete( &msg_context_global_key );
#ifndef NDEBUG
        vlc_threadvar_delete( &thread_object_key );
#endif
    }
    i_initializations--;

#if defined( LIBVLC_USE_PTHREAD )
    pthread_mutex_unlock( &once_mutex );
#endif
}

#if defined (__GLIBC__) && (__GLIBC_MINOR__ < 6)
/* This is not prototyped under glibc, though it exists. */
int pthread_mutexattr_setkind_np( pthread_mutexattr_t *attr, int kind );
#endif

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
int vlc_mutex_init( vlc_mutex_t *p_mutex )
{
#if defined( LIBVLC_USE_PTHREAD )
    pthread_mutexattr_t attr;
    int                 i_result;

    pthread_mutexattr_init( &attr );

# ifndef NDEBUG
    /* Create error-checking mutex to detect problems more easily. */
#  if defined (__GLIBC__) && (__GLIBC_MINOR__ < 6)
    pthread_mutexattr_setkind_np( &attr, PTHREAD_MUTEX_ERRORCHECK_NP );
#  else
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
#  endif
# endif
    i_result = pthread_mutex_init( p_mutex, &attr );
    pthread_mutexattr_destroy( &attr );
    return i_result;
#elif defined( UNDER_CE )
    InitializeCriticalSection( &p_mutex->csection );
    return 0;

#elif defined( WIN32 )
    *p_mutex = CreateMutex( 0, FALSE, 0 );
    return (*p_mutex != NULL) ? 0 : ENOMEM;

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
 * vlc_mutex_init: initialize a recursive mutex (Do not use)
 *****************************************************************************/
int vlc_mutex_init_recursive( vlc_mutex_t *p_mutex )
{
#if defined( LIBVLC_USE_PTHREAD )
    pthread_mutexattr_t attr;
    int                 i_result;

    pthread_mutexattr_init( &attr );
#  if defined (__GLIBC__) && (__GLIBC_MINOR__ < 6)
    pthread_mutexattr_setkind_np( &attr, PTHREAD_MUTEX_RECURSIVE_NP );
#  else
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
#  endif
    i_result = pthread_mutex_init( p_mutex, &attr );
    pthread_mutexattr_destroy( &attr );
    return( i_result );
#elif defined( WIN32 )
    /* Create mutex returns a recursive mutex */
    *p_mutex = CreateMutex( 0, FALSE, 0 );
    return (*p_mutex != NULL) ? 0 : ENOMEM;
#else
# error Unimplemented!
#endif
}


/*****************************************************************************
 * vlc_mutex_destroy: destroy a mutex, inner version
 *****************************************************************************/
void __vlc_mutex_destroy( const char * psz_file, int i_line, vlc_mutex_t *p_mutex )
{
#if defined( LIBVLC_USE_PTHREAD )
    int val = pthread_mutex_destroy( p_mutex );
    VLC_THREAD_ASSERT ("destroying mutex");

#elif defined( UNDER_CE )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    DeleteCriticalSection( &p_mutex->csection );

#elif defined( WIN32 )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    CloseHandle( *p_mutex );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    if( p_mutex->init == 9999 )
        delete_sem( p_mutex->lock );

    p_mutex->init = 0;

#endif
}

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
int __vlc_cond_init( vlc_cond_t *p_condvar )
{
#if defined( LIBVLC_USE_PTHREAD )
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

    ret = pthread_cond_init (p_condvar, &attr);
    pthread_condattr_destroy (&attr);
    return ret;

#elif defined( UNDER_CE ) || defined( WIN32 )
    /* Create an auto-reset event. */
    *p_condvar = CreateEvent( NULL,   /* no security */
                              FALSE,  /* auto-reset event */
                              FALSE,  /* start non-signaled */
                              NULL ); /* unnamed */
    return *p_condvar ? 0 : ENOMEM;

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
void __vlc_cond_destroy( const char * psz_file, int i_line, vlc_cond_t *p_condvar )
{
#if defined( LIBVLC_USE_PTHREAD )
    int val = pthread_cond_destroy( p_condvar );
    VLC_THREAD_ASSERT ("destroying condition");

#elif defined( UNDER_CE ) || defined( WIN32 )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    CloseHandle( *p_condvar );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    p_condvar->init = 0;

#endif
}

/*****************************************************************************
 * vlc_tls_create: create a thread-local variable
 *****************************************************************************/
int vlc_threadvar_create( vlc_threadvar_t *p_tls, void (*destr) (void *) )
{
    int i_ret;

#if defined( LIBVLC_USE_PTHREAD )
    i_ret =  pthread_key_create( p_tls, destr );
#elif defined( UNDER_CE )
    i_ret = ENOSYS;
#elif defined( WIN32 )
    /* FIXME: remember/use the destr() callback and stop leaking whatever */
    *p_tls = TlsAlloc();
    i_ret = (*p_tls == TLS_OUT_OF_INDEXES) ? EAGAIN : 0;
#else
# error Unimplemented!
#endif
    return i_ret;
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
#if defined( LIBVLC_USE_PTHREAD )
    pthread_key_delete (*p_tls);
#elif defined( UNDER_CE )
#elif defined( WIN32 )
    TlsFree (*p_tls);
#else
# error Unimplemented!
#endif
}

#if defined (LIBVLC_USE_PTHREAD)
#elif defined (WIN32)
static unsigned __stdcall vlc_entry (void *data)
{
    vlc_thread_t self = data;
    self->data = self->entry (self->data);
    return 0;
}
#endif

/**
 * Creates and starts new thread.
 *
 * @param p_handle [OUT] pointer to write the handle of the created thread to
 * @param entry entry point for the thread
 * @param data data parameter given to the entry point
 * @param priority thread priority value
 * @return 0 on success, a standard error code on error.
 */
int vlc_clone (vlc_thread_t *p_handle, void * (*entry) (void *), void *data,
               int priority)
{
    int ret;

#if defined( LIBVLC_USE_PTHREAD )
    pthread_attr_t attr;
    pthread_attr_init (&attr);

    /* Block the signals that signals interface plugin handles.
     * If the LibVLC caller wants to handle some signals by itself, it should
     * block these before whenever invoking LibVLC. And it must obviously not
     * start the VLC signals interface plugin.
     *
     * LibVLC will normally ignore any interruption caused by an asynchronous
     * signal during a system call. But there may well be some buggy cases
     * where it fails to handle EINTR (bug reports welcome). Some underlying
     * libraries might also not handle EINTR properly.
     */
    sigset_t oldset;
    {
        sigset_t set;
        sigemptyset (&set);
        sigdelset (&set, SIGHUP);
        sigaddset (&set, SIGINT);
        sigaddset (&set, SIGQUIT);
        sigaddset (&set, SIGTERM);

        sigaddset (&set, SIGPIPE); /* We don't want this one, really! */
        pthread_sigmask (SIG_BLOCK, &set, &oldset);
    }
    {
        struct sched_param sp = { .sched_priority = priority, };
        int policy;

        if (sp.sched_priority <= 0)
            sp.sched_priority += sched_get_priority_max (policy = SCHED_OTHER);
        else
            sp.sched_priority += sched_get_priority_min (policy = SCHED_RR);

        pthread_attr_setschedpolicy (&attr, policy);
        pthread_attr_setschedparam (&attr, &sp);
    }

    ret = pthread_create (p_handle, &attr, entry, data);
    pthread_sigmask (SIG_SETMASK, &oldset, NULL);
    pthread_attr_destroy (&attr);

#elif defined( WIN32 ) || defined( UNDER_CE )
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with
     * memory leaks and the signal functions not working (see Microsoft
     * Knowledge Base, article 104641) */
    HANDLE hThread;
    vlc_thread_t th = malloc (sizeof (*p_handle));

    if (th == NULL)
        return ENOMEM;

    th->data = data;
    th->entry = entry;
#if defined( UNDER_CE )
    hThread = CreateThread (NULL, 0, vlc_entry, th, CREATE_SUSPENDED, NULL);
#else
    hThread = (HANDLE)(uintptr_t)
        _beginthreadex (NULL, 0, vlc_entry, th, CREATE_SUSPENDED, NULL);
#endif

    if (hThread)
    {
        /* Thread closes the handle when exiting, duplicate it here
         * to be on the safe side when joining. */
        if (!DuplicateHandle (GetCurrentProcess (), hThread,
                              GetCurrentProcess (), &th->handle, 0, FALSE,
                              DUPLICATE_SAME_ACCESS))
        {
            CloseHandle (hThread);
            free (th);
            return ENOMEM;
        }

        ResumeThread (hThread);
        if (priority)
            SetThreadPriority (hThread, priority);

        ret = 0;
        *p_handle = th;
    }
    else
    {
        ret = errno;
        free (th);
    }

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    *p_handle = spawn_thread( entry, psz_name, priority, data );
    ret = resume_thread( *p_handle );

#endif
    return ret;
}

#if defined (WIN32)
/* APC procedure for thread cancellation */
static void CALLBACK vlc_cancel_self (ULONG_PTR dummy)
{
    (void)dummy;
    vlc_control_cancel (VLC_DO_CANCEL);
}
#endif

/**
 * Marks a thread as cancelled. Next time the target thread reaches a
 * cancellation point (while not having disabled cancellation), it will
 * run its cancellation cleanup handler, the thread variable destructors, and
 * terminate. vlc_join() must be used afterward regardless of a thread being
 * cancelled or not.
 */
void vlc_cancel (vlc_thread_t thread_id)
{
#if defined (LIBVLC_USE_PTHREAD)
    pthread_cancel (thread_id);
#elif defined (WIN32)
    QueueUserAPC (vlc_cancel_self, thread_id->handle, 0);
#endif
}

/**
 * Waits for a thread to complete (if needed), and destroys it.
 * This is a cancellation point; in case of cancellation, the join does _not_
 * occur.
 *
 * @param handle thread handle
 * @param p_result [OUT] pointer to write the thread return value or NULL
 * @return 0 on success, a standard error code otherwise.
 */
int vlc_join (vlc_thread_t handle, void **result)
{
#if defined( LIBVLC_USE_PTHREAD )
    return pthread_join (handle, result);

#elif defined( UNDER_CE ) || defined( WIN32 )
    do
        vlc_testcancel ();
    while (WaitForSingleObjectEx (handle->handle, INFINITE, TRUE)
                                                        == WAIT_IO_COMPLETION);

    CloseHandle (handle->handle);
    if (result)
        *result = handle->data;
    free (handle);
    return 0;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    int32_t exit_value;
    ret = (B_OK == wait_for_thread( p_priv->thread_id, &exit_value ));
    if( !ret && result )
        *result = (void *)exit_value;

    return ret;
#endif
}


struct vlc_thread_boot
{
    void * (*entry) (vlc_object_t *);
    vlc_object_t *object;
};

static void *thread_entry (void *data)
{
    vlc_object_t *obj = ((struct vlc_thread_boot *)data)->object;
    void *(*func) (vlc_object_t *) = ((struct vlc_thread_boot *)data)->entry;

    free (data);
#ifndef NDEBUG
    vlc_threadvar_set (&thread_object_key, obj);
#endif
    msg_Dbg (obj, "thread started");
    func (obj);
    msg_Dbg (obj, "thread ended");

    return NULL;
}

/*****************************************************************************
 * vlc_thread_create: create a thread, inner version
 *****************************************************************************
 * Note that i_priority is only taken into account on platforms supporting
 * userland real-time priority threads.
 *****************************************************************************/
int __vlc_thread_create( vlc_object_t *p_this, const char * psz_file, int i_line,
                         const char *psz_name, void * ( *func ) ( vlc_object_t * ),
                         int i_priority, bool b_wait )
{
    int i_ret;
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    struct vlc_thread_boot *boot = malloc (sizeof (*boot));
    if (boot == NULL)
        return errno;
    boot->entry = func;
    boot->object = p_this;

    vlc_object_lock( p_this );

    /* Make sure we don't re-create a thread if the object has already one */
    assert( !p_priv->b_thread );

#if defined( LIBVLC_USE_PTHREAD )
#ifndef __APPLE__
    if( config_GetInt( p_this, "rt-priority" ) > 0 )
#endif
    {
        /* Hack to avoid error msg */
        if( config_GetType( p_this, "rt-offset" ) )
            i_priority += config_GetInt( p_this, "rt-offset" );
    }
#endif

    i_ret = vlc_clone( &p_priv->thread_id, thread_entry, boot, i_priority );
    if( i_ret == 0 )
    {
        if( b_wait )
        {
            msg_Dbg( p_this, "waiting for thread initialization" );
            vlc_object_wait( p_this );
        }

        p_priv->b_thread = true;
        msg_Dbg( p_this, "thread %lu (%s) created at priority %d (%s:%d)",
                 (unsigned long)p_priv->thread_id, psz_name, i_priority,
                 psz_file, i_line );
    }
    else
    {
        errno = i_ret;
        msg_Err( p_this, "%s thread could not be created at %s:%d (%m)",
                         psz_name, psz_file, i_line );
    }

    vlc_object_unlock( p_this );
    return i_ret;
}

/*****************************************************************************
 * vlc_thread_set_priority: set the priority of the current thread when we
 * couldn't set it in vlc_thread_create (for instance for the main thread)
 *****************************************************************************/
int __vlc_thread_set_priority( vlc_object_t *p_this, const char * psz_file,
                               int i_line, int i_priority )
{
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    if( !p_priv->b_thread )
    {
        msg_Err( p_this, "couldn't set priority of non-existent thread" );
        return ESRCH;
    }

#if defined( LIBVLC_USE_PTHREAD )
# ifndef __APPLE__
    if( config_GetInt( p_this, "rt-priority" ) > 0 )
# endif
    {
        int i_error, i_policy;
        struct sched_param param;

        memset( &param, 0, sizeof(struct sched_param) );
        if( config_GetType( p_this, "rt-offset" ) )
            i_priority += config_GetInt( p_this, "rt-offset" );
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

#elif defined( WIN32 ) || defined( UNDER_CE )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    if( !SetThreadPriority(p_priv->thread_id->handle, i_priority) )
    {
        msg_Warn( p_this, "couldn't set a faster priority" );
        return 1;
    }

#endif

    return 0;
}

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits, inner version
 *****************************************************************************/
void __vlc_thread_join( vlc_object_t *p_this, const char * psz_file, int i_line )
{
    vlc_object_internals_t *p_priv = vlc_internals( p_this );
    int i_ret = 0;

#if defined( LIBVLC_USE_PTHREAD )
    /* Make sure we do return if we are calling vlc_thread_join()
     * from the joined thread */
    if (pthread_equal (pthread_self (), p_priv->thread_id))
    {
        msg_Warn (p_this, "joining the active thread (VLC might crash)");
        i_ret = pthread_detach (p_priv->thread_id);
    }
    else
        i_ret = vlc_join (p_priv->thread_id, NULL);

#elif defined( UNDER_CE ) || defined( WIN32 )
    HANDLE hThread;
    FILETIME create_ft, exit_ft, kernel_ft, user_ft;
    int64_t real_time, kernel_time, user_time;

    if( ! DuplicateHandle(GetCurrentProcess(),
            p_priv->thread_id->handle,
            GetCurrentProcess(),
            &hThread,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS) )
    {
        p_priv->b_thread = false;
        i_ret = GetLastError();
        goto error;
    }

    vlc_join( p_priv->thread_id, NULL );

    if( GetThreadTimes( hThread, &create_ft, &exit_ft, &kernel_ft, &user_ft ) )
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
                 "real %"PRId64"m%fs, kernel %"PRId64"m%fs, user %"PRId64"m%fs",
                 real_time/60/1000000,
                 (double)((real_time%(60*1000000))/1000000.0),
                 kernel_time/60/1000000,
                 (double)((kernel_time%(60*1000000))/1000000.0),
                 user_time/60/1000000,
                 (double)((user_time%(60*1000000))/1000000.0) );
    }
    CloseHandle( hThread );
error:

#else
    i_ret = vlc_join( p_priv->thread_id, NULL );

#endif

    if( i_ret )
    {
        errno = i_ret;
        msg_Err( p_this, "thread_join(%lu) failed at %s:%d (%m)",
                         (unsigned long)p_priv->thread_id, psz_file, i_line );
    }
    else
        msg_Dbg( p_this, "thread %lu joined (%s:%d)",
                         (unsigned long)p_priv->thread_id, psz_file, i_line );

    p_priv->b_thread = false;
}

void vlc_thread_cancel (vlc_object_t *obj)
{
    vlc_object_internals_t *priv = vlc_internals (obj);

    if (priv->b_thread)
        vlc_cancel (priv->thread_id);
}

typedef struct vlc_cleanup_t
{
    struct vlc_cleanup_t *next;
    void                (*proc) (void *);
    void                 *data;
} vlc_cleanup_t;

typedef struct vlc_cancel_t
{
    vlc_cleanup_t *cleaners;
    bool           killable;
    bool           killed;
} vlc_cancel_t;

void vlc_control_cancel (int cmd, ...)
{
#ifdef LIBVLC_USE_PTHREAD
    (void) cmd;
    assert (0);
#else
    va_list ap;

    va_start (ap, cmd);

    vlc_cancel_t *nfo = vlc_threadvar_get (&cancel_key);
    if (nfo == NULL)
    {
        nfo = malloc (sizeof (*nfo));
        if (nfo == NULL)
            abort ();
        nfo->cleaners = NULL;
        nfo->killed = false;
        nfo->killable = true;
    }

    switch (cmd)
    {
        case VLC_SAVE_CANCEL:
        {
            int *p_state = va_arg (ap, int *);
            *p_state = nfo->killable;
            nfo->killable = false;
            break;
        }

        case VLC_RESTORE_CANCEL:
        {
            int state = va_arg (ap, int);
            nfo->killable = state != 0;
            break;
        }

        case VLC_TEST_CANCEL:
            if (nfo->killable && nfo->killed)
            {
                for (vlc_cleanup_t *p = nfo->cleaners; p != NULL; p = p->next)
                     p->proc (p->data);
                free (nfo);
#if defined (LIBVLC_USE_PTHREAD)
                pthread_exit (PTHREAD_CANCELLED);
#elif defined (WIN32)
                _endthread ();
#else
# error Not implemented!
#endif
            }
            break;

        case VLC_DO_CANCEL:
            nfo->killed = true;
            break;
    }
    va_end (ap);
#endif
}
