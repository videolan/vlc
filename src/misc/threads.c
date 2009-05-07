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
 *          Rémi Denis-Courmont
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

#if defined( LIBVLC_USE_PTHREAD )
# include <sched.h>
# ifdef __linux__
#  include <sys/syscall.h> /* SYS_gettid */
# endif
#else
static vlc_threadvar_t cancel_key;
#endif

#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif

#ifdef __APPLE__
# include <sys/time.h> /* gettimeofday in vlc_cond_timedwait */
#endif

/**
 * Print a backtrace to the standard error for debugging purpose.
 */
void vlc_trace (const char *fn, const char *file, unsigned line)
{
     fprintf (stderr, "at %s:%u in %s\n", file, line, fn);
     fflush (stderr); /* needed before switch to low-level I/O */
#ifdef HAVE_BACKTRACE
     void *stack[20];
     int len = backtrace (stack, sizeof (stack) / sizeof (stack[0]));
     backtrace_symbols_fd (stack, len, 2);
#endif
#ifndef WIN32
     fsync (2);
#endif
}

static inline unsigned long vlc_threadid (void)
{
#if defined (LIBVLC_USE_PTHREAD)
# if defined (__linux__)
     return syscall (SYS_gettid);

# else
     union { pthread_t th; unsigned long int i; } v = { };
     v.th = pthread_self ();
     return v.i;

#endif
#elif defined (WIN32)
     return GetCurrentThreadId ();

#else
     return 0;

#endif
}

#ifndef NDEBUG
/*****************************************************************************
 * vlc_thread_fatal: Report an error from the threading layer
 *****************************************************************************
 * This is mostly meant for debugging.
 *****************************************************************************/
static void
vlc_thread_fatal (const char *action, int error,
                  const char *function, const char *file, unsigned line)
{
    fprintf (stderr, "LibVLC fatal error %s (%d) in thread %lu ",
             action, error, vlc_threadid ());
    vlc_trace (function, file, line);

    /* Sometimes strerror_r() crashes too, so make sure we print an error
     * message before we invoke it */
#ifdef __GLIBC__
    /* Avoid the strerror_r() prototype brain damage in glibc */
    errno = error;
    fprintf (stderr, " Error message: %m\n");
#elif !defined (WIN32)
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

# define VLC_THREAD_ASSERT( action ) \
    if (val) vlc_thread_fatal (action, val, __func__, __FILE__, __LINE__)
#else
# define VLC_THREAD_ASSERT( action ) ((void)val)
#endif

/**
 * Per-thread cancellation data
 */
#ifndef LIBVLC_USE_PTHREAD_CANCEL
typedef struct vlc_cancel_t
{
    vlc_cleanup_t *cleaners;
    bool           killable;
    bool           killed;
# ifdef UNDER_CE
    HANDLE         cancel_event;
# endif
} vlc_cancel_t;

# ifndef UNDER_CE
#  define VLC_CANCEL_INIT { NULL, true, false }
# else
#  define VLC_CANCEL_INIT { NULL, true, false, NULL }
# endif
#endif

#ifdef UNDER_CE
static void CALLBACK vlc_cancel_self (ULONG_PTR dummy);

static DWORD vlc_cancelable_wait (DWORD count, const HANDLE *handles,
                                  DWORD delay)
{
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
    {
        /* Main thread - cannot be cancelled anyway */
        return WaitForMultipleObjects (count, handles, FALSE, delay);
    }
    HANDLE new_handles[count + 1];
    memcpy(new_handles, handles, count * sizeof(HANDLE));
    new_handles[count] = nfo->cancel_event;
    DWORD result = WaitForMultipleObjects (count + 1, new_handles, FALSE,
                                           delay);
    if (result == WAIT_OBJECT_0 + count)
    {
        vlc_cancel_self (NULL);
        return WAIT_IO_COMPLETION;
    }
    else
    {
        return result;
    }
}

DWORD SleepEx (DWORD dwMilliseconds, BOOL bAlertable)
{
    if (bAlertable)
    {
        DWORD result = vlc_cancelable_wait (0, NULL, dwMilliseconds);
        return (result == WAIT_TIMEOUT) ? 0 : WAIT_IO_COMPLETION;
    }
    else
    {
        Sleep(dwMilliseconds);
        return 0;
    }
}

DWORD WaitForSingleObjectEx (HANDLE hHandle, DWORD dwMilliseconds,
                             BOOL bAlertable)
{
    if (bAlertable)
    {
        /* The MSDN documentation specifies different return codes,
         * but in practice they are the same. We just check that it
         * remains so. */
#if WAIT_ABANDONED != WAIT_ABANDONED_0
# error Windows headers changed, code needs to be rewritten!
#endif
        return vlc_cancelable_wait (1, &hHandle, dwMilliseconds);
    }
    else
    {
        return WaitForSingleObject (hHandle, dwMilliseconds);
    }
}

DWORD WaitForMultipleObjectsEx (DWORD nCount, const HANDLE *lpHandles,
                                BOOL bWaitAll, DWORD dwMilliseconds,
                                BOOL bAlertable)
{
    if (bAlertable)
    {
        /* We do not support the bWaitAll case */
        assert (! bWaitAll);
        return vlc_cancelable_wait (nCount, lpHandles, dwMilliseconds);
    }
    else
    {
        return WaitForMultipleObjects (nCount, lpHandles, bWaitAll,
                                       dwMilliseconds);
    }
}
#endif

#ifdef WIN32
static vlc_mutex_t super_mutex;

BOOL WINAPI DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    (void) hinstDll;
    (void) lpvReserved;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            vlc_mutex_init (&super_mutex);
            vlc_threadvar_create (&cancel_key, free);
            break;

        case DLL_PROCESS_DETACH:
            vlc_threadvar_delete( &cancel_key );
            vlc_mutex_destroy (&super_mutex);
            break;
    }
    return TRUE;
}
#endif

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

#elif defined( WIN32 )
    /* This creates a recursive mutex. This is OK as fast mutexes have
     * no defined behavior in case of recursive locking. */
    InitializeCriticalSection (&p_mutex->mutex);
    p_mutex->initialized = 1;
    return 0;

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
    InitializeCriticalSection( &p_mutex->mutex );
    p_mutex->initialized = 1;
    return 0;

#endif
}


/**
 * Destroys a mutex. The mutex must not be locked.
 *
 * @param p_mutex mutex to destroy
 * @return always succeeds
 */
void vlc_mutex_destroy (vlc_mutex_t *p_mutex)
{
#if defined( LIBVLC_USE_PTHREAD )
    int val = pthread_mutex_destroy( p_mutex );
    VLC_THREAD_ASSERT ("destroying mutex");

#elif defined( WIN32 )
    assert (InterlockedExchange (&p_mutex->initialized, -1) == 1);
    DeleteCriticalSection (&p_mutex->mutex);

#endif
}

#if defined(LIBVLC_USE_PTHREAD) && !defined(NDEBUG)
# ifdef HAVE_VALGRIND_VALGRIND_H
#  include <valgrind/valgrind.h>
# else
#  define RUNNING_ON_VALGRIND (0)
# endif

void vlc_assert_locked (vlc_mutex_t *p_mutex)
{
    if (RUNNING_ON_VALGRIND > 0)
        return;
    assert (pthread_mutex_lock (p_mutex) == EDEADLK);
}
#endif

/**
 * Acquires a mutex. If needed, waits for any other thread to release it.
 * Beware of deadlocks when locking multiple mutexes at the same time,
 * or when using mutexes from callbacks.
 * This function is not a cancellation-point.
 *
 * @param p_mutex mutex initialized with vlc_mutex_init() or
 *                vlc_mutex_init_recursive()
 */
void vlc_mutex_lock (vlc_mutex_t *p_mutex)
{
#if defined(LIBVLC_USE_PTHREAD)
    int val = pthread_mutex_lock( p_mutex );
    VLC_THREAD_ASSERT ("locking mutex");

#elif defined( WIN32 )
    if (InterlockedCompareExchange (&p_mutex->initialized, 0, 0) == 0)
    { /* ^^ We could also lock super_mutex all the time... sluggish */
        assert (p_mutex != &super_mutex); /* this one cannot be static */

        vlc_mutex_lock (&super_mutex);
        if (InterlockedCompareExchange (&p_mutex->initialized, 0, 0) == 0)
            vlc_mutex_init (p_mutex);
        /* FIXME: destroy the mutex some time... */
        vlc_mutex_unlock (&super_mutex);
    }
    assert (InterlockedExchange (&p_mutex->initialized, 1) == 1);
    EnterCriticalSection (&p_mutex->mutex);

#endif
}

/**
 * Acquires a mutex if and only if it is not currently held by another thread.
 * This function never sleeps and can be used in delay-critical code paths.
 * This function is not a cancellation-point.
 *
 * <b>Beware</b>: If this function fails, then the mutex is held... by another
 * thread. The calling thread must deal with the error appropriately. That
 * typically implies postponing the operations that would have required the
 * mutex. If the thread cannot defer those operations, then it must use
 * vlc_mutex_lock(). If in doubt, use vlc_mutex_lock() instead.
 *
 * @param p_mutex mutex initialized with vlc_mutex_init() or
 *                vlc_mutex_init_recursive()
 * @return 0 if the mutex could be acquired, an error code otherwise.
 */
int vlc_mutex_trylock (vlc_mutex_t *p_mutex)
{
#if defined(LIBVLC_USE_PTHREAD)
    int val = pthread_mutex_trylock( p_mutex );

    if (val != EBUSY)
        VLC_THREAD_ASSERT ("locking mutex");
    return val;

#elif defined( WIN32 )
    if (InterlockedCompareExchange (&p_mutex->initialized, 0, 0) == 0)
    { /* ^^ We could also lock super_mutex all the time... sluggish */
        assert (p_mutex != &super_mutex); /* this one cannot be static */

        vlc_mutex_lock (&super_mutex);
        if (InterlockedCompareExchange (&p_mutex->initialized, 0, 0) == 0)
            vlc_mutex_init (p_mutex);
        /* FIXME: destroy the mutex some time... */
        vlc_mutex_unlock (&super_mutex);
    }
    assert (InterlockedExchange (&p_mutex->initialized, 1) == 1);
    return TryEnterCriticalSection (&p_mutex->mutex) ? 0 : EBUSY;

#endif
}

/**
 * Releases a mutex (or crashes if the mutex is not locked by the caller).
 * @param p_mutex mutex locked with vlc_mutex_lock().
 */
void vlc_mutex_unlock (vlc_mutex_t *p_mutex)
{
#if defined(LIBVLC_USE_PTHREAD)
    int val = pthread_mutex_unlock( p_mutex );
    VLC_THREAD_ASSERT ("unlocking mutex");

#elif defined( WIN32 )
    assert (InterlockedExchange (&p_mutex->initialized, 1) == 1);
    LeaveCriticalSection (&p_mutex->mutex);

#endif
}

/*****************************************************************************
 * vlc_cond_init: initialize a condition variable
 *****************************************************************************/
int vlc_cond_init( vlc_cond_t *p_condvar )
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

#elif defined( WIN32 )
    /* Create a manual-reset event (manual reset is needed for broadcast). */
    *p_condvar = CreateEvent( NULL, TRUE, FALSE, NULL );
    return *p_condvar ? 0 : ENOMEM;

#endif
}

/**
 * Destroys a condition variable. No threads shall be waiting or signaling the
 * condition.
 * @param p_condvar condition variable to destroy
 */
void vlc_cond_destroy (vlc_cond_t *p_condvar)
{
#if defined( LIBVLC_USE_PTHREAD )
    int val = pthread_cond_destroy( p_condvar );
    VLC_THREAD_ASSERT ("destroying condition");

#elif defined( WIN32 )
    CloseHandle( *p_condvar );

#endif
}

/**
 * Wakes up one thread waiting on a condition variable, if any.
 * @param p_condvar condition variable
 */
void vlc_cond_signal (vlc_cond_t *p_condvar)
{
#if defined(LIBVLC_USE_PTHREAD)
    int val = pthread_cond_signal( p_condvar );
    VLC_THREAD_ASSERT ("signaling condition variable");

#elif defined( WIN32 )
    /* NOTE: This will cause a broadcast, that is wrong.
     * This will also wake up the next waiting thread if no thread are yet
     * waiting, which is also wrong. However both of these issues are allowed
     * by the provision for spurious wakeups. Better have too many wakeups
     * than too few (= deadlocks). */
    SetEvent (*p_condvar);

#endif
}

/**
 * Wakes up all threads (if any) waiting on a condition variable.
 * @param p_cond condition variable
 */
void vlc_cond_broadcast (vlc_cond_t *p_condvar)
{
#if defined (LIBVLC_USE_PTHREAD)
    pthread_cond_broadcast (p_condvar);

#elif defined (WIN32)
    SetEvent (*p_condvar);

#endif
}

/**
 * Waits for a condition variable. The calling thread will be suspended until
 * another thread calls vlc_cond_signal() or vlc_cond_broadcast() on the same
 * condition variable, the thread is cancelled with vlc_cancel(), or the
 * system causes a "spurious" unsolicited wake-up.
 *
 * A mutex is needed to wait on a condition variable. It must <b>not</b> be
 * a recursive mutex. Although it is possible to use the same mutex for
 * multiple condition, it is not valid to use different mutexes for the same
 * condition variable at the same time from different threads.
 *
 * In case of thread cancellation, the mutex is always locked before
 * cancellation proceeds.
 *
 * The canonical way to use a condition variable to wait for event foobar is:
 @code
   vlc_mutex_lock (&lock);
   mutex_cleanup_push (&lock); // release the mutex in case of cancellation

   while (!foobar)
       vlc_cond_wait (&wait, &lock);

   --- foobar is now true, do something about it here --

   vlc_cleanup_run (); // release the mutex
  @endcode
 *
 * @param p_condvar condition variable to wait on
 * @param p_mutex mutex which is unlocked while waiting,
 *                then locked again when waking up.
 * @param deadline <b>absolute</b> timeout
 *
 * @return 0 if the condition was signaled, an error code in case of timeout.
 */
void vlc_cond_wait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex)
{
#if defined(LIBVLC_USE_PTHREAD)
    int val = pthread_cond_wait( p_condvar, p_mutex );
    VLC_THREAD_ASSERT ("waiting on condition");

#elif defined( WIN32 )
    DWORD result;

    do
    {
        vlc_testcancel ();
        LeaveCriticalSection (&p_mutex->mutex);
        result = WaitForSingleObjectEx (*p_condvar, INFINITE, TRUE);
        EnterCriticalSection (&p_mutex->mutex);
    }
    while (result == WAIT_IO_COMPLETION);

    assert (result != WAIT_ABANDONED); /* another thread failed to cleanup! */
    assert (result != WAIT_FAILED);
    ResetEvent (*p_condvar);

#endif
}

/**
 * Waits for a condition variable up to a certain date.
 * This works like vlc_cond_wait(), except for the additional timeout.
 *
 * @param p_condvar condition variable to wait on
 * @param p_mutex mutex which is unlocked while waiting,
 *                then locked again when waking up.
 * @param deadline <b>absolute</b> timeout
 *
 * @return 0 if the condition was signaled, an error code in case of timeout.
 */
int vlc_cond_timedwait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex,
                        mtime_t deadline)
{
#if defined(LIBVLC_USE_PTHREAD)
#ifdef __APPLE__
    /* mdate() is mac_absolute_time on osx, which we must convert to do
     * the same base than gettimeofday() on which pthread_cond_timedwait
     * counts on. */
    mtime_t oldbase = mdate();
    struct timeval tv;
    gettimeofday(&tv, NULL);
    mtime_t newbase = (mtime_t)tv.tv_sec * 1000000 + (mtime_t) tv.tv_usec;
    deadline = deadline - oldbase + newbase;
#endif
    lldiv_t d = lldiv( deadline, CLOCK_FREQ );
    struct timespec ts = { d.quot, d.rem * (1000000000 / CLOCK_FREQ) };

    int val = pthread_cond_timedwait (p_condvar, p_mutex, &ts);
    if (val != ETIMEDOUT)
        VLC_THREAD_ASSERT ("timed-waiting on condition");
    return val;

#elif defined( WIN32 )
    DWORD result;

    do
    {
        vlc_testcancel ();

        mtime_t total = (deadline - mdate ())/1000;
        if( total < 0 )
            total = 0;

        DWORD delay = (total > 0x7fffffff) ? 0x7fffffff : total;
        LeaveCriticalSection (&p_mutex->mutex);
        result = WaitForSingleObjectEx (*p_condvar, delay, TRUE);
        EnterCriticalSection (&p_mutex->mutex);
    }
    while (result == WAIT_IO_COMPLETION);

    assert (result != WAIT_ABANDONED);
    assert (result != WAIT_FAILED);
    ResetEvent (*p_condvar);

    return (result == WAIT_OBJECT_0) ? 0 : ETIMEDOUT;

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
#elif defined( WIN32 )
    TlsFree (*p_tls);
#else
# error Unimplemented!
#endif
}

/**
 * Sets a thread-local variable.
 * @param key thread-local variable key (created with vlc_threadvar_create())
 * @param value new value for the variable for the calling thread
 * @return 0 on success, a system error code otherwise.
 */
int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
#if defined(LIBVLC_USE_PTHREAD)
    return pthread_setspecific (key, value);
#elif defined( UNDER_CE ) || defined( WIN32 )
    return TlsSetValue (key, value) ? ENOMEM : 0;
#else
# error Unimplemented!
#endif
}

/**
 * Gets the value of a thread-local variable for the calling thread.
 * This function cannot fail.
 * @return the value associated with the given variable for the calling
 * or NULL if there is no value.
 */
void *vlc_threadvar_get (vlc_threadvar_t key)
{
#if defined(LIBVLC_USE_PTHREAD)
    return pthread_getspecific (key);
#elif defined( UNDER_CE ) || defined( WIN32 )
    return TlsGetValue (key);
#else
# error Unimplemented!
#endif
}

#if defined (LIBVLC_USE_PTHREAD)
#elif defined (WIN32)
static unsigned __stdcall vlc_entry (void *data)
{
    vlc_cancel_t cancel_data = VLC_CANCEL_INIT;
    vlc_thread_t self = data;
#ifdef UNDER_CE
    cancel_data.cancel_event = self->cancel_event;
#endif

    vlc_threadvar_set (cancel_key, &cancel_data);
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

#if defined (_POSIX_PRIORITY_SCHEDULING) && (_POSIX_PRIORITY_SCHEDULING >= 0) \
 && defined (_POSIX_THREAD_PRIORITY_SCHEDULING) \
 && (_POSIX_THREAD_PRIORITY_SCHEDULING >= 0)
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
#else
    (void) priority;
#endif

    /* The thread stack size.
     * The lower the value, the less address space per thread, the highest
     * maximum simultaneous threads per process. Too low values will cause
     * stack overflows and weird crashes. Set with caution. Also keep in mind
     * that 64-bits platforms consume more stack than 32-bits one.
     *
     * Thanks to on-demand paging, thread stack size only affects address space
     * consumption. In terms of memory, threads only use what they need
     * (rounded up to the page boundary).
     *
     * For example, on Linux i386, the default is 2 mega-bytes, which supports
     * about 320 threads per processes. */
#define VLC_STACKSIZE (128 * sizeof (void *) * 1024)

#ifdef VLC_STACKSIZE
    ret = pthread_attr_setstacksize (&attr, VLC_STACKSIZE);
    assert (ret == 0); /* fails iif VLC_STACKSIZE is invalid */
#endif

    ret = pthread_create (p_handle, &attr, entry, data);
    pthread_sigmask (SIG_SETMASK, &oldset, NULL);
    pthread_attr_destroy (&attr);

#elif defined( WIN32 ) || defined( UNDER_CE )
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with
     * memory leaks and the signal functions not working (see Microsoft
     * Knowledge Base, article 104641) */
    HANDLE hThread;
    vlc_thread_t th = malloc (sizeof (*th));

    if (th == NULL)
        return ENOMEM;

    th->data = data;
    th->entry = entry;
#if defined( UNDER_CE )
    th->cancel_event = CreateEvent (NULL, FALSE, FALSE, NULL);
    if (th->cancel_event == NULL)
    {
        free(th);
        return errno;
    }
    hThread = CreateThread (NULL, 128*1024, vlc_entry, th, CREATE_SUSPENDED, NULL);
#else
    hThread = (HANDLE)(uintptr_t)
        _beginthreadex (NULL, 0, vlc_entry, th, CREATE_SUSPENDED, NULL);
#endif

    if (hThread)
    {
#ifndef UNDER_CE
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
#else
        th->handle = hThread;
#endif

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
#if defined (LIBVLC_USE_PTHREAD_CANCEL)
    pthread_cancel (thread_id);
#elif defined (UNDER_CE)
    SetEvent (thread_id->cancel_event);
#elif defined (WIN32)
    QueueUserAPC (vlc_cancel_self, thread_id->handle, 0);
#else
#   warning vlc_cancel is not implemented!
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
void vlc_join (vlc_thread_t handle, void **result)
{
#if defined( LIBVLC_USE_PTHREAD )
    int val = pthread_join (handle, result);
    VLC_THREAD_ASSERT ("joining thread");

#elif defined( UNDER_CE ) || defined( WIN32 )
    do
        vlc_testcancel ();
    while (WaitForSingleObjectEx (handle->handle, INFINITE, TRUE)
                                                        == WAIT_IO_COMPLETION);

    CloseHandle (handle->handle);
    if (result)
        *result = handle->data;
#if defined( UNDER_CE )
    CloseHandle (handle->cancel_event);
#endif
    free (handle);

#endif
}

/**
 * Save the current cancellation state (enabled or disabled), then disable
 * cancellation for the calling thread.
 * This function must be called before entering a piece of code that is not
 * cancellation-safe, unless it can be proven that the calling thread will not
 * be cancelled.
 * @return Previous cancellation state (opaque value for vlc_restorecancel()).
 */
int vlc_savecancel (void)
{
    int state;

#if defined (LIBVLC_USE_PTHREAD_CANCEL)
    int val = pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);
    VLC_THREAD_ASSERT ("saving cancellation");

#else
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
        return false; /* Main thread - cannot be cancelled anyway */

     state = nfo->killable;
     nfo->killable = false;

#endif
    return state;
}

/**
 * Restore the cancellation state for the calling thread.
 * @param state previous state as returned by vlc_savecancel().
 * @return Nothing, always succeeds.
 */
void vlc_restorecancel (int state)
{
#if defined (LIBVLC_USE_PTHREAD_CANCEL)
# ifndef NDEBUG
    int oldstate, val;

    val = pthread_setcancelstate (state, &oldstate);
    /* This should fail if an invalid value for given for state */
    VLC_THREAD_ASSERT ("restoring cancellation");

    if (oldstate != PTHREAD_CANCEL_DISABLE)
         vlc_thread_fatal ("restoring cancellation while not disabled", EINVAL,
                           __func__, __FILE__, __LINE__);
# else
    pthread_setcancelstate (state, NULL);
# endif

#else
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    assert (state == false || state == true);

    if (nfo == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    assert (!nfo->killable);
    nfo->killable = state != 0;

#endif
}

/**
 * Issues an explicit deferred cancellation point.
 * This has no effect if thread cancellation is disabled.
 * This can be called when there is a rather slow non-sleeping operation.
 * This is also used to force a cancellation point in a function that would
 * otherwise "not always" be a one (block_FifoGet() is an example).
 */
void vlc_testcancel (void)
{
#if defined (LIBVLC_USE_PTHREAD_CANCEL)
    pthread_testcancel ();

#else
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    if (nfo->killable && nfo->killed)
    {
        for (vlc_cleanup_t *p = nfo->cleaners; p != NULL; p = p->next)
             p->proc (p->data);
# if defined (LIBVLC_USE_PTHREAD)
        pthread_exit (PTHREAD_CANCELLED);
# elif defined (UNDER_CE)
        ExitThread(0);
# elif defined (WIN32)
        _endthread ();
# else
#  error Not implemented!
# endif
    }
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
    msg_Dbg (obj, "thread started");
    func (obj);
    msg_Dbg (obj, "thread ended");

    return NULL;
}

#undef vlc_thread_create
/*****************************************************************************
 * vlc_thread_create: create a thread
 *****************************************************************************
 * Note that i_priority is only taken into account on platforms supporting
 * userland real-time priority threads.
 *****************************************************************************/
int vlc_thread_create( vlc_object_t *p_this, const char * psz_file, int i_line,
                       const char *psz_name, void *(*func) ( vlc_object_t * ),
                       int i_priority )
{
    int i_ret;
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    struct vlc_thread_boot *boot = malloc (sizeof (*boot));
    if (boot == NULL)
        return errno;
    boot->entry = func;
    boot->object = p_this;

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

    p_priv->b_thread = true;
    i_ret = vlc_clone( &p_priv->thread_id, thread_entry, boot, i_priority );
    if( i_ret == 0 )
        msg_Dbg( p_this, "thread (%s) created at priority %d (%s:%d)",
                 psz_name, i_priority, psz_file, i_line );
    else
    {
        p_priv->b_thread = false;
        errno = i_ret;
        msg_Err( p_this, "%s thread could not be created at %s:%d (%m)",
                         psz_name, psz_file, i_line );
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
void __vlc_thread_join( vlc_object_t *p_this )
{
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

#if defined( LIBVLC_USE_PTHREAD )
    vlc_join (p_priv->thread_id, NULL);

#elif defined( UNDER_CE ) || defined( WIN32 )
    HANDLE hThread;
    FILETIME create_ft, exit_ft, kernel_ft, user_ft;
    int64_t real_time, kernel_time, user_time;

#ifndef UNDER_CE
    if( ! DuplicateHandle(GetCurrentProcess(),
            p_priv->thread_id->handle,
            GetCurrentProcess(),
            &hThread,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS) )
    {
        p_priv->b_thread = false;
        return; /* We have a problem! */
    }
#else
    hThread = p_priv->thread_id->handle;
#endif

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

#else
    vlc_join( p_priv->thread_id, NULL );

#endif

    p_priv->b_thread = false;
}

void vlc_thread_cancel (vlc_object_t *obj)
{
    vlc_object_internals_t *priv = vlc_internals (obj);

    if (priv->b_thread)
        vlc_cancel (priv->thread_id);
}

void vlc_control_cancel (int cmd, ...)
{
    /* NOTE: This function only modifies thread-specific data, so there is no
     * need to lock anything. */
#ifdef LIBVLC_USE_PTHREAD_CANCEL
    (void) cmd;
    assert (0);
#else
    va_list ap;

    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
    {
#ifdef WIN32
        /* Main thread - cannot be cancelled anyway */
        return;
#else
        nfo = malloc (sizeof (*nfo));
        if (nfo == NULL)
            return; /* Uho! Expect problems! */
        *nfo = VLC_CANCEL_INIT;
        vlc_threadvar_set (cancel_key, nfo);
#endif
    }

    va_start (ap, cmd);
    switch (cmd)
    {
        case VLC_DO_CANCEL:
            nfo->killed = true;
            break;

        case VLC_CLEANUP_PUSH:
        {
            /* cleaner is a pointer to the caller stack, no need to allocate
             * and copy anything. As a nice side effect, this cannot fail. */
            vlc_cleanup_t *cleaner = va_arg (ap, vlc_cleanup_t *);
            cleaner->next = nfo->cleaners;
            nfo->cleaners = cleaner;
            break;
        }

        case VLC_CLEANUP_POP:
        {
            nfo->cleaners = nfo->cleaners->next;
            break;
        }
    }
    va_end (ap);
#endif
}
