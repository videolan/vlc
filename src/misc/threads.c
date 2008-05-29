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

/**
 * Object running the current thread
 */
static vlc_threadvar_t thread_object_key;

vlc_object_t *vlc_threadobj (void)
{
    return vlc_threadvar_get (&thread_object_key);
}

vlc_threadvar_t msg_context_global_key;

#if defined(LIBVLC_USE_PTHREAD)
static inline unsigned long vlc_threadid (void)
{
     union { pthread_t th; unsigned long int i; } v = { };
     v.th = pthread_self ();
     return v.i;
}

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
    fflush (stderr);

    /* Sometimes strerror_r() crashes too, so make sure we print an error
     * message before we invoke it */
#ifdef __GLIBC__
    /* Avoid the strerror_r() prototype brain damage in glibc */
    errno = error;
    fprintf (stderr, " Error message: %m\n");
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
    abort ();
}
#else
void vlc_pthread_fatal (const char *action, int error,
                        const char *file, unsigned line)
{
    (void)action; (void)error; (void)file; (void)line;
    abort();
}
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
        p_root = vlc_custom_create( NULL, sizeof( *p_root ),
                                    VLC_OBJECT_GENERIC, "root" );
        if( p_root == NULL )
        {
            i_ret = VLC_ENOMEM;
            goto out;
        }

        /* We should be safe now. Do all the initialization stuff we want. */
        vlc_threadvar_create( &thread_object_key, NULL );
        vlc_threadvar_create( &msg_context_global_key, msg_StackDestroy );
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
        vlc_threadvar_delete( &msg_context_global_key );
        vlc_threadvar_delete( &thread_object_key );
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
    /* Initialize counter */
    p_condvar->i_waiting_threads = 0;

    /* Create an auto-reset event. */
    p_condvar->event = CreateEvent( NULL,   /* no security */
                                    FALSE,  /* auto-reset event */
                                    FALSE,  /* start non-signaled */
                                    NULL ); /* unnamed */
    return !p_condvar->event;

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

    CloseHandle( p_condvar->event );

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

struct vlc_thread_boot
{
    void * (*entry) (void *);
    vlc_object_t *object;
};

#if defined (LIBVLC_USE_PTHREAD)
# define THREAD_RTYPE void *
# define THREAD_RVAL  NULL
#elif defined (WIN32)
# define THREAD_RTYPE __stdcall unsigned
# define THREAD_RVAL 0
#endif

static THREAD_RTYPE thread_entry (void *data)
{
    vlc_object_t *obj = ((struct vlc_thread_boot *)data)->object;
    void *(*func) (void *) = ((struct vlc_thread_boot *)data)->entry;

    free (data);
    vlc_threadvar_set (&thread_object_key, obj);
    msg_Dbg (obj, "thread started");
    func (obj);
    msg_Dbg (obj, "thread ended");
    return THREAD_RVAL;
}

/*****************************************************************************
 * vlc_thread_create: create a thread, inner version
 *****************************************************************************
 * Note that i_priority is only taken into account on platforms supporting
 * userland real-time priority threads.
 *****************************************************************************/
int __vlc_thread_create( vlc_object_t *p_this, const char * psz_file, int i_line,
                         const char *psz_name, void * ( *func ) ( void * ),
                         int i_priority, bool b_wait )
{
    int i_ret;
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    struct vlc_thread_boot *boot = malloc (sizeof (*boot));
    if (boot == NULL)
        return errno;
    boot->entry = func;
    boot->object = p_this;

    vlc_mutex_lock( &p_this->object_lock );

#if defined( LIBVLC_USE_PTHREAD )
    sigset_t set, oldset;

    /* We really don't want signals to (literaly) interrupt our blocking I/O
     * system calls. SIGPIPE is especially bad, as it can be caused by remote
     * peers through connected sockets. Generally, we cannot know which signals
     * are handled by the main program. Also, external LibVLC bindings tend not
     * to setup a proper signal mask before invoking LibVLC.
     * Hence, we hereby block all signals, except those for which blocking is
     * undefined, as listed below. Note that SIGKILL and SIGSTOP need not be
     * listed (see the documentation for pthread_sigmask) here. */
    sigfillset (&set);
    sigdelset (&set, SIGFPE);
    sigdelset (&set, SIGILL);
    sigdelset (&set, SIGSEGV);
    sigdelset (&set, SIGBUS);
    pthread_sigmask (SIG_BLOCK, &set, &oldset);

    i_ret = pthread_create( &p_priv->thread_id, NULL, thread_entry, boot );
    pthread_sigmask (SIG_SETMASK, &oldset, NULL);

#ifndef __APPLE__
    if( config_GetInt( p_this, "rt-priority" ) > 0 )
#endif
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
#ifndef __APPLE__
    else
        i_priority = 0;
#endif

#elif defined( WIN32 ) || defined( UNDER_CE )
    {
        /* When using the MSVCRT C library you have to use the _beginthreadex
         * function instead of CreateThread, otherwise you'll end up with
         * memory leaks and the signal functions not working (see Microsoft
         * Knowledge Base, article 104641) */
#if defined( UNDER_CE )
        HANDLE hThread = CreateThread( NULL, 0, thread_entry,
                                       (LPVOID)boot, CREATE_SUSPENDED,
                                        NULL );
#else
        HANDLE hThread = (HANDLE)(uintptr_t)
            _beginthreadex( NULL, 0, thread_entry, boot,
                            CREATE_SUSPENDED, NULL );
#endif
        p_priv->thread_id = hThread;
        ResumeThread(hThread);
    }

    i_ret = ( p_priv->thread_id ? 0 : errno );

    if( !i_ret && i_priority )
    {
        if( !SetThreadPriority(p_priv->thread_id, i_priority) )
        {
            msg_Warn( p_this, "couldn't set a faster priority" );
            i_priority = 0;
        }
    }

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    p_priv->thread_id = spawn_thread( (thread_func)thread_entry, psz_name,
                                      i_priority, p_data );
    i_ret = resume_thread( p_priv->thread_id );

#endif

    if( i_ret == 0 )
    {
        if( b_wait )
        {
            msg_Dbg( p_this, "waiting for thread completion" );
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

    vlc_mutex_unlock( &p_this->object_lock );
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

#elif defined( WIN32 ) || defined( UNDER_CE )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    if( !p_priv->thread_id )
        p_priv->thread_id = GetCurrentThread();
    if( !SetThreadPriority(p_priv->thread_id, i_priority) )
    {
        msg_Warn( p_this, "couldn't set a faster priority" );
        return 1;
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
    vlc_object_internals_t *p_priv = vlc_internals( p_this );
    int i_ret = 0;

#if defined( LIBVLC_USE_PTHREAD )
    /* Make sure we do return if we are calling vlc_thread_join()
     * from the joined thread */
    if (pthread_equal (pthread_self (), p_priv->thread_id))
        i_ret = pthread_detach (p_priv->thread_id);
    else
        i_ret = pthread_join (p_priv->thread_id, NULL);

#elif defined( UNDER_CE ) || defined( WIN32 )
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
            p_priv->thread_id,
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

    WaitForSingleObject( hThread, INFINITE );

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

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    int32_t exit_value;
    i_ret = (B_OK == wait_for_thread( p_priv->thread_id, &exit_value ));

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
