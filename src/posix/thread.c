/*****************************************************************************
 * thread.c : pthread back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2009 VLC authors and VideoLAN
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Clément Sténac
 *          Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_atomic.h>

#include "libvlc.h"
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <sys/types.h>
#include <unistd.h> /* fsync() */
#include <pthread.h>
#include <sched.h>

#ifdef __linux__
# include <sys/syscall.h> /* SYS_gettid */
#endif
#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif
#ifdef __APPLE__
# include <mach/mach_init.h> /* mach_task_self in semaphores */
#endif
#if defined(__SunOS)
# include <sys/processor.h>
# include <sys/pset.h>
#endif

#if !defined (_POSIX_TIMERS)
# define _POSIX_TIMERS (-1)
#endif
#if !defined (_POSIX_CLOCK_SELECTION)
/* Clock selection was defined in 2001 and became mandatory in 2008. */
# define _POSIX_CLOCK_SELECTION (-1)
#endif
#if !defined (_POSIX_MONOTONIC_CLOCK)
# define _POSIX_MONOTONIC_CLOCK (-1)
#endif

#if (_POSIX_TIMERS > 0)
static unsigned vlc_clock_prec;

# if (_POSIX_MONOTONIC_CLOCK > 0) && (_POSIX_CLOCK_SELECTION > 0)
/* Compile-time POSIX monotonic clock support */
#  define vlc_clock_id (CLOCK_MONOTONIC)

# elif (_POSIX_MONOTONIC_CLOCK == 0) && (_POSIX_CLOCK_SELECTION > 0)
/* Run-time POSIX monotonic clock support (see clock_setup() below) */
static clockid_t vlc_clock_id;

# else
/* No POSIX monotonic clock support */
#   define vlc_clock_id (CLOCK_REALTIME)
#   warning Monotonic clock not available. Expect timing issues.

# endif /* _POSIX_MONOTONIC_CLOKC */

static void vlc_clock_setup_once (void)
{
# if (_POSIX_MONOTONIC_CLOCK == 0)
    long val = sysconf (_SC_MONOTONIC_CLOCK);
    assert (val != 0);
    vlc_clock_id = (val < 0) ? CLOCK_REALTIME : CLOCK_MONOTONIC;
# endif

    struct timespec res;
    if (unlikely(clock_getres (vlc_clock_id, &res) != 0 || res.tv_sec != 0))
        abort ();
    vlc_clock_prec = (res.tv_nsec + 500) / 1000;
}

static pthread_once_t vlc_clock_once = PTHREAD_ONCE_INIT;

# define vlc_clock_setup() \
    pthread_once(&vlc_clock_once, vlc_clock_setup_once)

#else /* _POSIX_TIMERS */

# include <sys/time.h> /* gettimeofday() */
# if defined (HAVE_DECL_NANOSLEEP) && !HAVE_DECL_NANOSLEEP
int nanosleep (struct timespec *, struct timespec *);
# endif

# define vlc_clock_setup() (void)0
# warning Monotonic clock not available. Expect timing issues.
#endif /* _POSIX_TIMERS */

static struct timespec mtime_to_ts (mtime_t date)
{
    lldiv_t d = lldiv (date, CLOCK_FREQ);
    struct timespec ts = { d.quot, d.rem * (1000000000 / CLOCK_FREQ) };

    return ts;
}

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
     fsync (2);
}

static inline unsigned long vlc_threadid (void)
{
#if defined (__linux__)
     /* glibc does not provide a call for this */
     return syscall (__NR_gettid);

#else
     union { pthread_t th; unsigned long int i; } v = { };
     v.th = pthread_self ();
     return v.i;

#endif
}

#ifndef NDEBUG
/**
 * Reports a fatal error from the threading layer, for debugging purposes.
 */
static void
vlc_thread_fatal (const char *action, int error,
                  const char *function, const char *file, unsigned line)
{
    int canc = vlc_savecancel ();
    fprintf (stderr, "LibVLC fatal error %s (%d) in thread %lu ",
             action, error, vlc_threadid ());
    vlc_trace (function, file, line);

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
            msg = "unknown (too big to display)";
            break;
        default:
            msg = "unknown (invalid error number)";
            break;
    }
    fprintf (stderr, " Error message: %s\n", msg);
#endif
    fflush (stderr);

    vlc_restorecancel (canc);
    abort ();
}

# define VLC_THREAD_ASSERT( action ) \
    if (unlikely(val)) \
        vlc_thread_fatal (action, val, __func__, __FILE__, __LINE__)
#else
# define VLC_THREAD_ASSERT( action ) ((void)val)
#endif

/**
 * Initializes a fast mutex.
 */
void vlc_mutex_init( vlc_mutex_t *p_mutex )
{
    pthread_mutexattr_t attr;

    if (unlikely(pthread_mutexattr_init (&attr)))
        abort();
#ifdef NDEBUG
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_DEFAULT);
#else
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_ERRORCHECK);
#endif
    if (unlikely(pthread_mutex_init (p_mutex, &attr)))
        abort();
    pthread_mutexattr_destroy( &attr );
}

/**
 * Initializes a recursive mutex.
 * \warning This is strongly discouraged. Please use normal mutexes.
 */
void vlc_mutex_init_recursive( vlc_mutex_t *p_mutex )
{
    pthread_mutexattr_t attr;

    if (unlikely(pthread_mutexattr_init (&attr)))
        abort();
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    if (unlikely(pthread_mutex_init (p_mutex, &attr)))
        abort();
    pthread_mutexattr_destroy( &attr );
}


/**
 * Destroys a mutex. The mutex must not be locked.
 *
 * @param p_mutex mutex to destroy
 * @return always succeeds
 */
void vlc_mutex_destroy (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_destroy( p_mutex );
    VLC_THREAD_ASSERT ("destroying mutex");
}

#ifndef NDEBUG
# ifdef HAVE_VALGRIND_VALGRIND_H
#  include <valgrind/valgrind.h>
# else
#  define RUNNING_ON_VALGRIND (0)
# endif

/**
 * Asserts that a mutex is locked by the calling thread.
 */
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
    int val = pthread_mutex_lock( p_mutex );
    VLC_THREAD_ASSERT ("locking mutex");
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
    int val = pthread_mutex_trylock( p_mutex );

    if (val != EBUSY)
        VLC_THREAD_ASSERT ("locking mutex");
    return val;
}

/**
 * Releases a mutex (or crashes if the mutex is not locked by the caller).
 * @param p_mutex mutex locked with vlc_mutex_lock().
 */
void vlc_mutex_unlock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_unlock( p_mutex );
    VLC_THREAD_ASSERT ("unlocking mutex");
}

/**
 * Initializes a condition variable.
 */
void vlc_cond_init (vlc_cond_t *p_condvar)
{
    pthread_condattr_t attr;

    if (unlikely(pthread_condattr_init (&attr)))
        abort ();
#if (_POSIX_CLOCK_SELECTION > 0)
    vlc_clock_setup ();
    pthread_condattr_setclock (&attr, vlc_clock_id);
#endif
    if (unlikely(pthread_cond_init (p_condvar, &attr)))
        abort ();
    pthread_condattr_destroy (&attr);
}

/**
 * Initializes a condition variable.
 * Contrary to vlc_cond_init(), the wall clock will be used as a reference for
 * the vlc_cond_timedwait() time-out parameter.
 */
void vlc_cond_init_daytime (vlc_cond_t *p_condvar)
{
    if (unlikely(pthread_cond_init (p_condvar, NULL)))
        abort ();
}

/**
 * Destroys a condition variable. No threads shall be waiting or signaling the
 * condition.
 * @param p_condvar condition variable to destroy
 */
void vlc_cond_destroy (vlc_cond_t *p_condvar)
{
    int val = pthread_cond_destroy( p_condvar );
    VLC_THREAD_ASSERT ("destroying condition");
}

/**
 * Wakes up one thread waiting on a condition variable, if any.
 * @param p_condvar condition variable
 */
void vlc_cond_signal (vlc_cond_t *p_condvar)
{
    int val = pthread_cond_signal( p_condvar );
    VLC_THREAD_ASSERT ("signaling condition variable");
}

/**
 * Wakes up all threads (if any) waiting on a condition variable.
 * @param p_cond condition variable
 */
void vlc_cond_broadcast (vlc_cond_t *p_condvar)
{
    pthread_cond_broadcast (p_condvar);
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
 */
void vlc_cond_wait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex)
{
    int val = pthread_cond_wait( p_condvar, p_mutex );
    VLC_THREAD_ASSERT ("waiting on condition");
}

/**
 * Waits for a condition variable up to a certain date.
 * This works like vlc_cond_wait(), except for the additional time-out.
 *
 * If the variable was initialized with vlc_cond_init(), the timeout has the
 * same arbitrary origin as mdate(). If the variable was initialized with
 * vlc_cond_init_daytime(), the timeout is expressed from the Unix epoch.
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
    struct timespec ts = mtime_to_ts (deadline);
    int val = pthread_cond_timedwait (p_condvar, p_mutex, &ts);
    if (val != ETIMEDOUT)
        VLC_THREAD_ASSERT ("timed-waiting on condition");
    return val;
}

/**
 * Initializes a semaphore.
 */
void vlc_sem_init (vlc_sem_t *sem, unsigned value)
{
#if defined(__APPLE__)
    if (unlikely(semaphore_create(mach_task_self(), sem, SYNC_POLICY_FIFO, value) != KERN_SUCCESS))
        abort ();
#else
    if (unlikely(sem_init (sem, 0, value)))
        abort ();
#endif
}

/**
 * Destroys a semaphore.
 */
void vlc_sem_destroy (vlc_sem_t *sem)
{
    int val;

#if defined(__APPLE__)
    if (likely(semaphore_destroy(mach_task_self(), *sem) == KERN_SUCCESS))
        return;

    val = EINVAL;
#else
    if (likely(sem_destroy (sem) == 0))
        return;

    val = errno;
#endif

    VLC_THREAD_ASSERT ("destroying semaphore");
}

/**
 * Increments the value of a semaphore.
 * @return 0 on success, EOVERFLOW in case of integer overflow
 */
int vlc_sem_post (vlc_sem_t *sem)
{
    int val;

#if defined(__APPLE__)
    if (likely(semaphore_signal(*sem) == KERN_SUCCESS))
        return 0;

    val = EINVAL;
#else
    if (likely(sem_post (sem) == 0))
        return 0;

    val = errno;
#endif

    if (unlikely(val != EOVERFLOW))
        VLC_THREAD_ASSERT ("unlocking semaphore");
    return val;
}

/**
 * Atomically wait for the semaphore to become non-zero (if needed),
 * then decrements it.
 */
void vlc_sem_wait (vlc_sem_t *sem)
{
    int val;

#if defined(__APPLE__)
    if (likely(semaphore_wait(*sem) == KERN_SUCCESS))
        return;

    val = EINVAL;
#else
    do
        if (likely(sem_wait (sem) == 0))
            return;
    while ((val = errno) == EINTR);
#endif

    VLC_THREAD_ASSERT ("locking semaphore");
}

/**
 * Initializes a read/write lock.
 */
void vlc_rwlock_init (vlc_rwlock_t *lock)
{
    if (unlikely(pthread_rwlock_init (lock, NULL)))
        abort ();
}

/**
 * Destroys an initialized unused read/write lock.
 */
void vlc_rwlock_destroy (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_destroy (lock);
    VLC_THREAD_ASSERT ("destroying R/W lock");
}

/**
 * Acquires a read/write lock for reading. Recursion is allowed.
 * @note This function may be a point of cancellation.
 */
void vlc_rwlock_rdlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_rdlock (lock);
    VLC_THREAD_ASSERT ("acquiring R/W lock for reading");
}

/**
 * Acquires a read/write lock for writing. Recursion is not allowed.
 * @note This function may be a point of cancellation.
 */
void vlc_rwlock_wrlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_wrlock (lock);
    VLC_THREAD_ASSERT ("acquiring R/W lock for writing");
}

/**
 * Releases a read/write lock.
 */
void vlc_rwlock_unlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_unlock (lock);
    VLC_THREAD_ASSERT ("releasing R/W lock");
}

/**
 * Allocates a thread-specific variable.
 * @param key where to store the thread-specific variable handle
 * @param destr a destruction callback. It is called whenever a thread exits
 * and the thread-specific variable has a non-NULL value.
 * @return 0 on success, a system error code otherwise. This function can
 * actually fail because there is a fixed limit on the number of
 * thread-specific variable in a process on most systems.
 */
int vlc_threadvar_create (vlc_threadvar_t *key, void (*destr) (void *))
{
    return pthread_key_create (key, destr);
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
    pthread_key_delete (*p_tls);
}

/**
 * Sets a thread-specific variable.
 * @param key thread-local variable key (created with vlc_threadvar_create())
 * @param value new value for the variable for the calling thread
 * @return 0 on success, a system error code otherwise.
 */
int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    return pthread_setspecific (key, value);
}

/**
 * Gets the value of a thread-local variable for the calling thread.
 * This function cannot fail.
 * @return the value associated with the given variable for the calling
 * or NULL if there is no value.
 */
void *vlc_threadvar_get (vlc_threadvar_t key)
{
    return pthread_getspecific (key);
}

static bool rt_priorities = false;
static int rt_offset;

void vlc_threads_setup (libvlc_int_t *p_libvlc)
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static bool initialized = false;

    vlc_mutex_lock (&lock);
    /* Initializes real-time priorities before any thread is created,
     * just once per process. */
    if (!initialized)
    {
#ifndef __APPLE__
        if (var_InheritBool (p_libvlc, "rt-priority"))
#endif
        {
            rt_offset = var_InheritInteger (p_libvlc, "rt-offset");
            rt_priorities = true;
        }
        initialized = true;
    }
    vlc_mutex_unlock (&lock);
}


static int vlc_clone_attr (vlc_thread_t *th, pthread_attr_t *attr,
                           void *(*entry) (void *), void *data, int priority)
{
    int ret;

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
    if (rt_priorities)
    {
        struct sched_param sp = { .sched_priority = priority + rt_offset, };
        int policy;

        if (sp.sched_priority <= 0)
            sp.sched_priority += sched_get_priority_max (policy = SCHED_OTHER);
        else
            sp.sched_priority += sched_get_priority_min (policy = SCHED_RR);

        pthread_attr_setschedpolicy (attr, policy);
        pthread_attr_setschedparam (attr, &sp);
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
    ret = pthread_attr_setstacksize (attr, VLC_STACKSIZE);
    assert (ret == 0); /* fails iif VLC_STACKSIZE is invalid */
#endif

    ret = pthread_create (th, attr, entry, data);
    pthread_sigmask (SIG_SETMASK, &oldset, NULL);
    pthread_attr_destroy (attr);
    return ret;
}

/**
 * Creates and starts new thread.
 *
 * The thread must be <i>joined</i> with vlc_join() to reclaim resources
 * when it is not needed anymore.
 *
 * @param th [OUT] pointer to write the handle of the created thread to
 *                 (mandatory, must be non-NULL)
 * @param entry entry point for the thread
 * @param data data parameter given to the entry point
 * @param priority thread priority value
 * @return 0 on success, a standard error code on error.
 */
int vlc_clone (vlc_thread_t *th, void *(*entry) (void *), void *data,
               int priority)
{
    pthread_attr_t attr;

    pthread_attr_init (&attr);
    return vlc_clone_attr (th, &attr, entry, data, priority);
}

/**
 * Waits for a thread to complete (if needed), then destroys it.
 * This is a cancellation point; in case of cancellation, the join does _not_
 * occur.
 * @warning
 * A thread cannot join itself (normally VLC will abort if this is attempted).
 * Also, a detached thread <b>cannot</b> be joined.
 *
 * @param handle thread handle
 * @param p_result [OUT] pointer to write the thread return value or NULL
 */
void vlc_join (vlc_thread_t handle, void **result)
{
    int val = pthread_join (handle, result);
    VLC_THREAD_ASSERT ("joining thread");
}

/**
 * Creates and starts new detached thread.
 * A detached thread cannot be joined. Its resources will be automatically
 * released whenever the thread exits (in particular, its call stack will be
 * reclaimed).
 *
 * Detached thread are particularly useful when some work needs to be done
 * asynchronously, that is likely to be completed much earlier than the thread
 * can practically be joined. In this case, thread detach can spare memory.
 *
 * A detached thread may be cancelled, so as to expedite its termination.
 * Be extremely careful if you do this: while a normal joinable thread can
 * safely be cancelled after it has already exited, cancelling an already
 * exited detached thread is undefined: The thread handle would is destroyed
 * immediately when the detached thread exits. So you need to ensure that the
 * detached thread is still running before cancellation is attempted.
 *
 * @warning Care must be taken that any resources used by the detached thread
 * remains valid until the thread completes.
 *
 * @note A detached thread must eventually exit just like another other
 * thread. In practice, LibVLC will wait for detached threads to exit before
 * it unloads the plugins.
 *
 * @param th [OUT] pointer to hold the thread handle, or NULL
 * @param entry entry point for the thread
 * @param data data parameter given to the entry point
 * @param priority thread priority value
 * @return 0 on success, a standard error code on error.
 */
int vlc_clone_detach (vlc_thread_t *th, void *(*entry) (void *), void *data,
                      int priority)
{
    vlc_thread_t dummy;
    pthread_attr_t attr;

    if (th == NULL)
        th = &dummy;

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    return vlc_clone_attr (th, &attr, entry, data, priority);
}

int vlc_set_priority (vlc_thread_t th, int priority)
{
#if defined (_POSIX_PRIORITY_SCHEDULING) && (_POSIX_PRIORITY_SCHEDULING >= 0) \
 && defined (_POSIX_THREAD_PRIORITY_SCHEDULING) \
 && (_POSIX_THREAD_PRIORITY_SCHEDULING >= 0)
    if (rt_priorities)
    {
        struct sched_param sp = { .sched_priority = priority + rt_offset, };
        int policy;

        if (sp.sched_priority <= 0)
            sp.sched_priority += sched_get_priority_max (policy = SCHED_OTHER);
        else
            sp.sched_priority += sched_get_priority_min (policy = SCHED_RR);

        if (pthread_setschedparam (th, policy, &sp))
            return VLC_EGENERIC;
    }
#else
    (void) th; (void) priority;
#endif
    return VLC_SUCCESS;
}

/**
 * Marks a thread as cancelled. Next time the target thread reaches a
 * cancellation point (while not having disabled cancellation), it will
 * run its cancellation cleanup handler, the thread variable destructors, and
 * terminate. vlc_join() must be used afterward regardless of a thread being
 * cancelled or not.
 */
void vlc_cancel (vlc_thread_t thread_id)
{
    pthread_cancel (thread_id);
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
    int val = pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);

    VLC_THREAD_ASSERT ("saving cancellation");
    return state;
}

/**
 * Restore the cancellation state for the calling thread.
 * @param state previous state as returned by vlc_savecancel().
 * @return Nothing, always succeeds.
 */
void vlc_restorecancel (int state)
{
#ifndef NDEBUG
    int oldstate, val;

    val = pthread_setcancelstate (state, &oldstate);
    /* This should fail if an invalid value for given for state */
    VLC_THREAD_ASSERT ("restoring cancellation");

    if (unlikely(oldstate != PTHREAD_CANCEL_DISABLE))
         vlc_thread_fatal ("restoring cancellation while not disabled", EINVAL,
                           __func__, __FILE__, __LINE__);
#else
    pthread_setcancelstate (state, NULL);
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
    pthread_testcancel ();
}

void vlc_control_cancel (int cmd, ...)
{
    (void) cmd;
    assert (0);
}

/**
 * Precision monotonic clock.
 *
 * In principles, the clock has a precision of 1 MHz. But the actual resolution
 * may be much lower, especially when it comes to sleeping with mwait() or
 * msleep(). Most general-purpose operating systems provide a resolution of
 * only 100 to 1000 Hz.
 *
 * @warning The origin date (time value "zero") is not specified. It is
 * typically the time the kernel started, but this is platform-dependent.
 * If you need wall clock time, use gettimeofday() instead.
 *
 * @return a timestamp in microseconds.
 */
mtime_t mdate (void)
{
#if (_POSIX_TIMERS > 0)
    struct timespec ts;

    vlc_clock_setup ();
    if (unlikely(clock_gettime (vlc_clock_id, &ts) != 0))
        abort ();

    return (INT64_C(1000000) * ts.tv_sec) + (ts.tv_nsec / 1000);

#else
    struct timeval tv;

    if (unlikely(gettimeofday (&tv, NULL) != 0))
        abort ();
    return (INT64_C(1000000) * tv.tv_sec) + tv.tv_usec;

#endif
}

#undef mwait
/**
 * Waits until a deadline (possibly later due to OS scheduling).
 * @param deadline timestamp to wait for (see mdate())
 */
void mwait (mtime_t deadline)
{
#if (_POSIX_CLOCK_SELECTION > 0)
    vlc_clock_setup ();
    /* If the deadline is already elapsed, or within the clock precision,
     * do not even bother the system timer. */
    deadline -= vlc_clock_prec;

    struct timespec ts = mtime_to_ts (deadline);

    while (clock_nanosleep (vlc_clock_id, TIMER_ABSTIME, &ts, NULL) == EINTR);

#else
    deadline -= mdate ();
    if (deadline > 0)
        msleep (deadline);

#endif
}

#undef msleep
/**
 * Waits for an interval of time.
 * @param delay how long to wait (in microseconds)
 */
void msleep (mtime_t delay)
{
    struct timespec ts = mtime_to_ts (delay);

#if (_POSIX_CLOCK_SELECTION > 0)
    vlc_clock_setup ();
    while (clock_nanosleep (vlc_clock_id, 0, &ts, &ts) == EINTR);

#else
    while (nanosleep (&ts, &ts) == -1)
        assert (errno == EINTR);

#endif
}


/**
 * Count CPUs.
 * @return number of available (logical) CPUs.
 */
unsigned vlc_GetCPUCount(void)
{
#if defined(HAVE_SCHED_GETAFFINITY)
    cpu_set_t cpu;

    CPU_ZERO(&cpu);
    if (sched_getaffinity (0, sizeof (cpu), &cpu) < 0)
        return 1;

    return CPU_COUNT (&cpu);

#elif defined(__SunOS)
    unsigned count = 0;
    int type;
    u_int numcpus;
    processor_info_t cpuinfo;

    processorid_t *cpulist = malloc (sizeof (*cpulist) * sysconf(_SC_NPROCESSORS_MAX));
    if (unlikely(cpulist == NULL))
        return 1;

    if (pset_info(PS_MYID, &type, &numcpus, cpulist) == 0)
    {
        for (u_int i = 0; i < numcpus; i++)
            if (processor_info (cpulist[i], &cpuinfo) == 0)
                count += (cpuinfo.pi_state == P_ONLINE);
    }
    else
        count = sysconf (_SC_NPROCESSORS_ONLN);
    free (cpulist);
    return count ? count : 1;
#elif defined(_SC_NPROCESSORS_CONF)
    return sysconf(_SC_NPROCESSORS_CONF);
#else
#   warning "vlc_GetCPUCount is not implemented for your platform"
    return 1;
#endif
}
