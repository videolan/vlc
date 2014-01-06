/*****************************************************************************
 * thread.c : pthread back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2013 VLC authors and VideoLAN
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Clément Sténac
 *          Rémi Denis-Courmont
 *          Felix Paul Kühne <fkuehne # videolan.org>
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
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include <pthread.h>
#include <mach/mach_init.h> /* mach_task_self in semaphores */
#include <mach/mach_time.h>
#include <execinfo.h>

static mach_timebase_info_data_t vlc_clock_conversion_factor;

static void vlc_clock_setup_once (void)
{
    if (unlikely(mach_timebase_info (&vlc_clock_conversion_factor) != 0))
        abort ();
}

static pthread_once_t vlc_clock_once = PTHREAD_ONCE_INIT;

#define vlc_clock_setup() \
    pthread_once(&vlc_clock_once, vlc_clock_setup_once)

static struct timespec mtime_to_ts (mtime_t date)
{
    lldiv_t d = lldiv (date, CLOCK_FREQ);
    struct timespec ts = { d.quot, d.rem * (1000000000 / CLOCK_FREQ) };

    return ts;
}

/* Print a backtrace to the standard error for debugging purpose. */
void vlc_trace (const char *fn, const char *file, unsigned line)
{
     fprintf (stderr, "at %s:%u in %s\n", file, line, fn);
     fflush (stderr); /* needed before switch to low-level I/O */
     void *stack[20];
     int len = backtrace (stack, sizeof (stack) / sizeof (stack[0]));
     backtrace_symbols_fd (stack, len, 2);
     fsync (2);
}

static inline unsigned long vlc_threadid (void)
{
     union { pthread_t th; unsigned long int i; } v = { };
     v.th = pthread_self ();
     return v.i;
}

#ifndef NDEBUG
/* Reports a fatal error from the threading layer, for debugging purposes. */
static void
vlc_thread_fatal (const char *action, int error,
                  const char *function, const char *file, unsigned line)
{
    int canc = vlc_savecancel ();
    fprintf (stderr, "LibVLC fatal error %s (%d) in thread %lu ",
             action, error, vlc_threadid ());
    vlc_trace (function, file, line);

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

/* Initializes a fast mutex. */
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

/* Initializes a recursive mutex.
 * warning: This is strongly discouraged. Please use normal mutexes. */
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


/* Destroys a mutex. The mutex must not be locked.
 *
 * parameter: p_mutex mutex to destroy
 * returns: always succeeds */
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

/* Asserts that a mutex is locked by the calling thread. */
void vlc_assert_locked (vlc_mutex_t *p_mutex)
{
    if (RUNNING_ON_VALGRIND > 0)
        return;
    assert (pthread_mutex_lock (p_mutex) == EDEADLK);
}
#endif

/* Acquires a mutex. If needed, waits for any other thread to release it.
 * Beware of deadlocks when locking multiple mutexes at the same time,
 * or when using mutexes from callbacks.
 * This function is not a cancellation-point.
 *
 * parameter: p_mutex mutex initialized with vlc_mutex_init() or
 *                vlc_mutex_init_recursive()
 */
void vlc_mutex_lock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_lock( p_mutex );
    VLC_THREAD_ASSERT ("locking mutex");
}

/* Acquires a mutex if and only if it is not currently held by another thread.
 * This function never sleeps and can be used in delay-critical code paths.
 * This function is not a cancellation-point.
 *
 * BEWARE: If this function fails, then the mutex is held... by another
 * thread. The calling thread must deal with the error appropriately. That
 * typically implies postponing the operations that would have required the
 * mutex. If the thread cannot defer those operations, then it must use
 * vlc_mutex_lock(). If in doubt, use vlc_mutex_lock() instead.
 *
 * parameter: p_mutex mutex initialized with vlc_mutex_init() or
 *                vlc_mutex_init_recursive()
 * returns: 0 if the mutex could be acquired, an error code otherwise. */
int vlc_mutex_trylock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_trylock( p_mutex );

    if (val != EBUSY)
        VLC_THREAD_ASSERT ("locking mutex");
    return val;
}

/* Release a mutex (or crashes if the mutex is not locked by the caller).
 * parameter p_mutex mutex locked with vlc_mutex_lock(). */
void vlc_mutex_unlock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_unlock( p_mutex );
    VLC_THREAD_ASSERT ("unlocking mutex");
}

enum
{
    VLC_CLOCK_MONOTONIC = 0,
    VLC_CLOCK_REALTIME,
};

/* Initialize a condition variable. */
void vlc_cond_init (vlc_cond_t *p_condvar)
{
    if (unlikely(pthread_cond_init (&p_condvar->cond, NULL)))
        abort ();
    p_condvar->clock = VLC_CLOCK_MONOTONIC;
}

/* Initialize a condition variable.
 * Contrary to vlc_cond_init(), the wall clock will be used as a reference for
 * the vlc_cond_timedwait() time-out parameter. */
void vlc_cond_init_daytime (vlc_cond_t *p_condvar)
{
    if (unlikely(pthread_cond_init (&p_condvar->cond, NULL)))
        abort ();
    p_condvar->clock = VLC_CLOCK_REALTIME;

}

/* Destroys a condition variable. No threads shall be waiting or signaling the
 * condition.
 * parameter: p_condvar condition variable to destroy */
void vlc_cond_destroy (vlc_cond_t *p_condvar)
{
    int val = pthread_cond_destroy (&p_condvar->cond);

    /* due to a faulty pthread implementation within Darwin 11 and
     * later condition variables cannot be destroyed without
     * terminating the application immediately.
     * This Darwin kernel issue is still present in version 13
     * and might not be resolved prior to Darwin 15.
     * radar://12496249
     *
     * To work-around this, we are just leaking the condition variable
     * which is acceptable due to VLC's low number of created variables
     * and its usually limited runtime.
     * Ideally, we should implement a re-useable pool.
     */
    if (val != 0) {
        #ifndef NDEBUG
        printf("pthread_cond_destroy returned %i\n", val);
        #endif

        if (val == EBUSY)
            return;
    }

    VLC_THREAD_ASSERT ("destroying condition");
}

/* Wake up one thread waiting on a condition variable, if any.
 * parameter: p_condvar condition variable */
void vlc_cond_signal (vlc_cond_t *p_condvar)
{
    int val = pthread_cond_signal (&p_condvar->cond);
    VLC_THREAD_ASSERT ("signaling condition variable");
}

/* Wake up all threads (if any) waiting on a condition variable.
 * parameter: p_cond condition variable */
void vlc_cond_broadcast (vlc_cond_t *p_condvar)
{
    pthread_cond_broadcast (&p_condvar->cond);
}

/* Wait for a condition variable. The calling thread will be suspended until
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
 sample code:
   vlc_mutex_lock (&lock);
   mutex_cleanup_push (&lock); // release the mutex in case of cancellation

   while (!foobar)
       vlc_cond_wait (&wait, &lock);

   --- foobar is now true, do something about it here --

   vlc_cleanup_run (); // release the mutex
 *
 * 1st parameter: p_condvar condition variable to wait on
 * 2nd parameter: p_mutex mutex which is unlocked while waiting,
 *                then locked again when waking up. */
void vlc_cond_wait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex)
{
    int val = pthread_cond_wait (&p_condvar->cond, p_mutex);
    VLC_THREAD_ASSERT ("waiting on condition");
}

/* Wait for a condition variable up to a certain date.
 * This works like vlc_cond_wait(), except for the additional time-out.
 *
 * If the variable was initialized with vlc_cond_init(), the timeout has the
 * same arbitrary origin as mdate(). If the variable was initialized with
 * vlc_cond_init_daytime(), the timeout is expressed from the Unix epoch.
 *
 * 1st parameter: p_condvar condition variable to wait on
 * 2nd parameter: p_mutex mutex which is unlocked while waiting,
 *                then locked again when waking up.
 * 3rd parameter: deadline <b>absolute</b> timeout
 *
 * returns 0 if the condition was signaled, an error code in case of timeout.
 */
int vlc_cond_timedwait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex,
                        mtime_t deadline)
{
    int val = 0;

    /*
     * Note that both pthread_cond_timedwait_relative_np and pthread_cond_timedwait
     * convert the given timeout to a mach absolute deadline, with system startup
     * as the time origin. There is no way you can change this behaviour.
     *
     * For more details, see: https://devforums.apple.com/message/931605
     */

    if (p_condvar->clock == VLC_CLOCK_MONOTONIC) {

        /* 
         * mdate() is the monotonic clock, pthread_cond_timedwait expects
         * origin of gettimeofday(). Use timedwait_relative_np() instead.
         */
        mtime_t base = mdate();
        deadline -= base;
        if (deadline < 0)
            deadline = 0;
        struct timespec ts = mtime_to_ts(deadline);

        val = pthread_cond_timedwait_relative_np(&p_condvar->cond, p_mutex, &ts);

    } else {
        /* variant for vlc_cond_init_daytime */
        assert (p_condvar->clock == VLC_CLOCK_REALTIME);

        /* 
         * FIXME: It is assumed, that in this case the system waits until the real
         * time deadline is passed, even if the real time is adjusted in between.
         * This is not fulfilled, as described above.
         */
        struct timespec ts = mtime_to_ts(deadline);

        val = pthread_cond_timedwait(&p_condvar->cond, p_mutex, &ts);
    }
    
    if (val != ETIMEDOUT)
        VLC_THREAD_ASSERT ("timed-waiting on condition");
    return val;
}

/* Initialize a semaphore. */
void vlc_sem_init (vlc_sem_t *sem, unsigned value)
{
    if (unlikely(semaphore_create(mach_task_self(), sem, SYNC_POLICY_FIFO, value) != KERN_SUCCESS))
        abort ();
}

/* Destroy a semaphore. */
void vlc_sem_destroy (vlc_sem_t *sem)
{
    int val;

    if (likely(semaphore_destroy(mach_task_self(), *sem) == KERN_SUCCESS))
        return;

    val = EINVAL;

    VLC_THREAD_ASSERT ("destroying semaphore");
}

/* Increment the value of a semaphore.
 * returns 0 on success, EOVERFLOW in case of integer overflow */
int vlc_sem_post (vlc_sem_t *sem)
{
    int val;

    if (likely(semaphore_signal(*sem) == KERN_SUCCESS))
        return 0;

    val = EINVAL;

    if (unlikely(val != EOVERFLOW))
        VLC_THREAD_ASSERT ("unlocking semaphore");
    return val;
}

/* Atomically wait for the semaphore to become non-zero (if needed),
 * then decrements it. */
void vlc_sem_wait (vlc_sem_t *sem)
{
    int val;

    if (likely(semaphore_wait(*sem) == KERN_SUCCESS))
        return;

    val = EINVAL;

    VLC_THREAD_ASSERT ("locking semaphore");
}

/* Initialize a read/write lock. */
void vlc_rwlock_init (vlc_rwlock_t *lock)
{
    if (unlikely(pthread_rwlock_init (lock, NULL)))
        abort ();
}

/* Destroy an initialized unused read/write lock. */
void vlc_rwlock_destroy (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_destroy (lock);
    VLC_THREAD_ASSERT ("destroying R/W lock");
}

/* Acquire a read/write lock for reading. Recursion is allowed.
 * Attention: This function may be a cancellation point. */
void vlc_rwlock_rdlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_rdlock (lock);
    VLC_THREAD_ASSERT ("acquiring R/W lock for reading");
}

/* Acquire a read/write lock for writing. Recursion is not allowed.
 * Attention: This function may be a cancellation point. */
void vlc_rwlock_wrlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_wrlock (lock);
    VLC_THREAD_ASSERT ("acquiring R/W lock for writing");
}

/* Release a read/write lock. */
void vlc_rwlock_unlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_unlock (lock);
    VLC_THREAD_ASSERT ("releasing R/W lock");
}

/* Allocates a thread-specific variable.
 * 1st parameter: key where to store the thread-specific variable handle
 * 2nd parameter: destr a destruction callback. It is called whenever a thread
 * exits and the thread-specific variable has a non-NULL value.
 * returns 0 on success, a system error code otherwise.
 *
 * This function can actually fail because there is a fixed limit on the number
 * of thread-specific variable in a process on most systems.
 */
int vlc_threadvar_create (vlc_threadvar_t *key, void (*destr) (void *))
{
    return pthread_key_create (key, destr);
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
    pthread_key_delete (*p_tls);
}

/* Set a thread-specific variable.
 * 1st parameter: key thread-local variable key
 *                (created with vlc_threadvar_create())
 * 2nd parameter: value new value for the variable for the calling thread
 * returns 0 on success, a system error code otherwise. */
int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    return pthread_setspecific (key, value);
}

/* Get the value of a thread-local variable for the calling thread.
 * This function cannot fail.
 * returns the value associated with the given variable for the calling
 * or NULL if there is no value. */
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
        rt_offset = var_InheritInteger (p_libvlc, "rt-offset");
        rt_priorities = true;
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

    (void) priority;

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

/* Create and start a new thread.
 *
 * The thread must be joined with vlc_join() to reclaim resources when it is 
 * not needed anymore.
 *
 * 1st parameter: th [OUT] pointer to write the handle of the created thread to
 *                 (mandatory, must be non-NULL)
 * 2nd parameter: entry entry point for the thread
 * 3rd parameter: data data parameter given to the entry point
 * 4th parameter: priority thread priority value
 * returns 0 on success, a standard error code on error. */
int vlc_clone (vlc_thread_t *th, void *(*entry) (void *), void *data,
               int priority)
{
    pthread_attr_t attr;

    pthread_attr_init (&attr);
    return vlc_clone_attr (th, &attr, entry, data, priority);
}

/* Wait for a thread to complete (if needed), then destroys it.
 * This is a cancellation point; in case of cancellation, the join does _not_
 * occur.
 * 
 * WARNING: A thread cannot join itself (normally VLC will abort if this is
 * attempted). Also, a detached thread cannot be joined.
 *
 * 1st parameter: handle thread handle
 * 2nd parameter: p_result - pointer to write the thread return value or NULL
 */
void vlc_join (vlc_thread_t handle, void **result)
{
    int val = pthread_join (handle, result);
    VLC_THREAD_ASSERT ("joining thread");
}

/* Create and start a new detached thread.
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
 * WARNING: Care must be taken that any resources used by the detached thread
 * remains valid until the thread completes.
 *
 * Attention: A detached thread must eventually exit just like another other
 * thread. In practice, LibVLC will wait for detached threads to exit before
 * it unloads the plugins.
 *
 * 1st parameter: th [OUT] pointer to hold the thread handle, or NULL
 * 2nd parameter: entry entry point for the thread
 * 3rd parameter: data data parameter given to the entry point
 * 4th parameter: priority thread priority value
 * returns 0 on success, a standard error code on error. */
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
    (void) th; (void) priority;
    return VLC_SUCCESS;
}

/* Marks a thread as cancelled. Next time the target thread reaches a
 * cancellation point (while not having disabled cancellation), it will
 * run its cancellation cleanup handler, the thread variable destructors, and
 * terminate. vlc_join() must be used afterward regardless of a thread being
 * cancelled or not. */
void vlc_cancel (vlc_thread_t thread_id)
{
    pthread_cancel (thread_id);
}

/* Save the current cancellation state (enabled or disabled), then disable
 * cancellation for the calling thread.
 * This function must be called before entering a piece of code that is not
 * cancellation-safe, unless it can be proven that the calling thread will not
 * be cancelled.
 * returns Previous cancellation state (opaque value for vlc_restorecancel()). */
int vlc_savecancel (void)
{
    int state;
    int val = pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);

    VLC_THREAD_ASSERT ("saving cancellation");
    return state;
}

/* Restore the cancellation state for the calling thread.
 * parameter: previous state as returned by vlc_savecancel(). */
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

/* Issues an explicit deferred cancellation point.
 * This has no effect if thread cancellation is disabled.
 * This can be called when there is a rather slow non-sleeping operation.
 * This is also used to force a cancellation point in a function that would
 * otherwise "not always" be a one (block_FifoGet() is an example). */
void vlc_testcancel (void)
{
    pthread_testcancel ();
}

void vlc_control_cancel (int cmd, ...)
{
    (void) cmd;
    assert (0);
}

/* Precision monotonic clock.
 *
 * In principles, the clock has a precision of 1 MHz. But the actual resolution
 * may be much lower, especially when it comes to sleeping with mwait() or
 * msleep(). Most general-purpose operating systems provide a resolution of
 * only 100 to 1000 Hz.
 *
 * WARNING: The origin date (time value "zero") is not specified. It is
 * typically the time the kernel started, but this is platform-dependent.
 * If you need wall clock time, use gettimeofday() instead.
 *
 * returns a timestamp in microseconds. */
mtime_t mdate (void)
{
    vlc_clock_setup();
    uint64_t date = mach_absolute_time();

    /* denom is uint32_t, switch to 64 bits to prevent overflow. */
    uint64_t denom = vlc_clock_conversion_factor.denom;

    /* Switch to microsecs */
    denom *= 1000LL;

    /* Split the division to prevent overflow */
    lldiv_t d = lldiv (vlc_clock_conversion_factor.numer, denom);

    return (d.quot * date) + ((d.rem * date) / denom);
}

#undef mwait
/* Wait until a deadline (possibly later due to OS scheduling).
 * parameter: deadline timestamp to wait for (see mdate()) */
void mwait (mtime_t deadline)
{
    deadline -= mdate ();
    if (deadline > 0)
        msleep (deadline);
}

#undef msleep
/* Wait for an interval of time.
 * parameter: delay how long to wait (in microseconds) */
void msleep (mtime_t delay)
{
    struct timespec ts = mtime_to_ts (delay);

    /* nanosleep uses mach_absolute_time and mach_wait_until internally,
       but also handles kernel errors. Thus we use just this. */
    while (nanosleep (&ts, &ts) == -1)
        assert (errno == EINTR);
}

/* Count CPUs.
 * returns the number of available (logical) CPUs. */
unsigned vlc_GetCPUCount(void)
{
    return sysconf(_SC_NPROCESSORS_CONF);
}
