/*****************************************************************************
 * vlc_threads.h : threads implementation for the VideoLAN client
 * This header provides portable declarations for mutexes & conditions
 *****************************************************************************
 * Copyright (C) 1999, 2002 VLC authors and VideoLAN
 * Copyright © 2007-2016 Rémi Denis-Courmont
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef VLC_THREADS_H_
#define VLC_THREADS_H_

/**
 * \ingroup os
 * \defgroup thread Threads and synchronization primitives
 * @{
 * \file
 * Thread primitive declarations
 */

/**
 * Issues an explicit deferred cancellation point.
 *
 * This has no effects if thread cancellation is disabled.
 * This can be called when there is a rather slow non-sleeping operation.
 * This is also used to force a cancellation point in a function that would
 * otherwise <em>not always</em> be one (block_FifoGet() is an example).
 */
VLC_API void vlc_testcancel(void);

#if defined (_WIN32)
# include <process.h>
# ifndef ETIMEDOUT
#  define ETIMEDOUT 10060 /* This is the value in winsock.h. */
# endif

typedef struct vlc_thread *vlc_thread_t;
# define VLC_THREAD_CANCELED NULL
# define LIBVLC_NEED_SLEEP
typedef struct
{
    bool dynamic;
    union
    {
        struct
        {
            bool locked;
            unsigned long contention;
        };
        CRITICAL_SECTION mutex;
    };
} vlc_mutex_t;
#define VLC_STATIC_MUTEX { false, { { false, 0 } } }
#define LIBVLC_NEED_CONDVAR
#define LIBVLC_NEED_SEMAPHORE
#define LIBVLC_NEED_RWLOCK
typedef struct vlc_threadvar *vlc_threadvar_t;
typedef struct vlc_timer *vlc_timer_t;

# define VLC_THREAD_PRIORITY_LOW      0
# define VLC_THREAD_PRIORITY_INPUT    THREAD_PRIORITY_ABOVE_NORMAL
# define VLC_THREAD_PRIORITY_AUDIO    THREAD_PRIORITY_HIGHEST
# define VLC_THREAD_PRIORITY_VIDEO    0
# define VLC_THREAD_PRIORITY_OUTPUT   THREAD_PRIORITY_ABOVE_NORMAL
# define VLC_THREAD_PRIORITY_HIGHEST  THREAD_PRIORITY_TIME_CRITICAL

static inline int vlc_poll(struct pollfd *fds, unsigned nfds, int timeout)
{
    int val;

    vlc_testcancel();
    val = poll(fds, nfds, timeout);
    if (val < 0)
        vlc_testcancel();
    return val;
}
# define poll(u,n,t) vlc_poll(u, n, t)

#elif defined (__OS2__)
# include <errno.h>

typedef struct vlc_thread *vlc_thread_t;
#define VLC_THREAD_CANCELED NULL
typedef struct
{
    bool dynamic;
    union
    {
        struct
        {
            bool locked;
            unsigned long contention;
        };
        HMTX hmtx;
    };
} vlc_mutex_t;
#define VLC_STATIC_MUTEX { false, { { false, 0 } } }
typedef struct
{
    HEV      hev;
    unsigned waiters;
    HEV      hevAck;
    unsigned signaled;
} vlc_cond_t;
#define VLC_STATIC_COND { NULLHANDLE, 0, NULLHANDLE, 0 }
#define LIBVLC_NEED_SEMAPHORE
#define LIBVLC_NEED_RWLOCK
typedef struct vlc_threadvar *vlc_threadvar_t;
typedef struct vlc_timer *vlc_timer_t;

# define VLC_THREAD_PRIORITY_LOW      0
# define VLC_THREAD_PRIORITY_INPUT \
                                    MAKESHORT(PRTYD_MAXIMUM / 2, PRTYC_REGULAR)
# define VLC_THREAD_PRIORITY_AUDIO    MAKESHORT(PRTYD_MAXIMUM, PRTYC_REGULAR)
# define VLC_THREAD_PRIORITY_VIDEO    0
# define VLC_THREAD_PRIORITY_OUTPUT \
                                    MAKESHORT(PRTYD_MAXIMUM / 2, PRTYC_REGULAR)
# define VLC_THREAD_PRIORITY_HIGHEST  MAKESHORT(0, PRTYC_TIMECRITICAL)

# define pthread_sigmask  sigprocmask

static inline int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    static int (*vlc_poll_os2)(struct pollfd *, unsigned, int) = NULL;

    if (!vlc_poll_os2)
    {
        HMODULE hmod;
        CHAR szFailed[CCHMAXPATH];

        if (DosLoadModule(szFailed, sizeof(szFailed), "vlccore", &hmod))
            return -1;

        if (DosQueryProcAddr(hmod, 0, "_vlc_poll_os2", (PFN *)&vlc_poll_os2))
            return -1;
    }

    return (*vlc_poll_os2)(fds, nfds, timeout);
}
# define poll(u,n,t) vlc_poll(u, n, t)

#elif defined (__ANDROID__)      /* pthreads subset without pthread_cancel() */
# include <unistd.h>
# include <pthread.h>
# include <poll.h>
# define LIBVLC_USE_PTHREAD_CLEANUP   1
# define LIBVLC_NEED_SLEEP
# define LIBVLC_NEED_CONDVAR
# define LIBVLC_NEED_SEMAPHORE
# define LIBVLC_NEED_RWLOCK

typedef struct vlc_thread *vlc_thread_t;
#define VLC_THREAD_CANCELED NULL
typedef pthread_mutex_t vlc_mutex_t;
#define VLC_STATIC_MUTEX PTHREAD_MUTEX_INITIALIZER

typedef pthread_key_t   vlc_threadvar_t;
typedef struct vlc_timer *vlc_timer_t;

# define VLC_THREAD_PRIORITY_LOW      0
# define VLC_THREAD_PRIORITY_INPUT    0
# define VLC_THREAD_PRIORITY_AUDIO    0
# define VLC_THREAD_PRIORITY_VIDEO    0
# define VLC_THREAD_PRIORITY_OUTPUT   0
# define VLC_THREAD_PRIORITY_HIGHEST  0

static inline int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    int val;

    do
    {
        int ugly_timeout = ((unsigned)timeout >= 50) ? 50 : timeout;
        if (timeout >= 0)
            timeout -= ugly_timeout;

        vlc_testcancel ();
        val = poll (fds, nfds, ugly_timeout);
    }
    while (val == 0 && timeout != 0);

    return val;
}

# define poll(u,n,t) vlc_poll(u, n, t)

#elif defined (__APPLE__)
# define _APPLE_C_SOURCE    1 /* Proper pthread semantics on OSX */
# include <unistd.h>
# include <pthread.h>
/* Unnamed POSIX semaphores not supported on Mac OS X */
# include <mach/semaphore.h>
# include <mach/task.h>
# define LIBVLC_USE_PTHREAD           1
# define LIBVLC_USE_PTHREAD_CLEANUP   1

typedef pthread_t       vlc_thread_t;
#define VLC_THREAD_CANCELED PTHREAD_CANCELED
typedef pthread_mutex_t vlc_mutex_t;
#define VLC_STATIC_MUTEX PTHREAD_MUTEX_INITIALIZER
typedef pthread_cond_t vlc_cond_t;
#define VLC_STATIC_COND PTHREAD_COND_INITIALIZER
typedef semaphore_t     vlc_sem_t;
typedef pthread_rwlock_t vlc_rwlock_t;
#define VLC_STATIC_RWLOCK PTHREAD_RWLOCK_INITIALIZER
typedef pthread_key_t   vlc_threadvar_t;
typedef struct vlc_timer *vlc_timer_t;

# define VLC_THREAD_PRIORITY_LOW      0
# define VLC_THREAD_PRIORITY_INPUT   22
# define VLC_THREAD_PRIORITY_AUDIO   22
# define VLC_THREAD_PRIORITY_VIDEO    0
# define VLC_THREAD_PRIORITY_OUTPUT  22
# define VLC_THREAD_PRIORITY_HIGHEST 22

#else /* POSIX threads */
# include <unistd.h> /* _POSIX_SPIN_LOCKS */
# include <pthread.h>
# include <semaphore.h>

/**
 * Whether LibVLC threads are based on POSIX threads.
 */
# define LIBVLC_USE_PTHREAD           1

/**
 * Whether LibVLC thread cancellation is based on POSIX threads.
 */
# define LIBVLC_USE_PTHREAD_CLEANUP   1

/**
 * Thread handle.
 */
typedef struct
{
    pthread_t handle;
} vlc_thread_t;

/**
 * Return value of a canceled thread.
 */
#define VLC_THREAD_CANCELED PTHREAD_CANCELED

/**
 * Mutex.
 *
 * Storage space for a mutual exclusion lock.
 */
typedef pthread_mutex_t vlc_mutex_t;

/**
 * Static initializer for (static) mutex.
 */
#define VLC_STATIC_MUTEX PTHREAD_MUTEX_INITIALIZER

/**
 * Condition variable.
 *
 * Storage space for a thread condition variable.
 */
typedef pthread_cond_t  vlc_cond_t;

/**
 * Static initializer for (static) condition variable.
 *
 * \note
 * The condition variable will use the default clock, which is OS-dependent.
 * Therefore, where timed waits are necessary the condition variable should
 * always be initialized dynamically explicit instead of using this
 * initializer.
 */
#define VLC_STATIC_COND  PTHREAD_COND_INITIALIZER

/**
 * Semaphore.
 *
 * Storage space for a thread-safe semaphore.
 */
typedef sem_t           vlc_sem_t;

/**
 * Read/write lock.
 *
 * Storage space for a slim reader/writer lock.
 */
typedef pthread_rwlock_t vlc_rwlock_t;

/**
 * Static initializer for (static) read/write lock.
 */
#define VLC_STATIC_RWLOCK PTHREAD_RWLOCK_INITIALIZER

/**
 * Thread-local key handle.
 */
typedef pthread_key_t   vlc_threadvar_t;

/**
 * Threaded timer handle.
 */
typedef struct vlc_timer *vlc_timer_t;

# define VLC_THREAD_PRIORITY_LOW      0
# define VLC_THREAD_PRIORITY_INPUT   10
# define VLC_THREAD_PRIORITY_AUDIO    5
# define VLC_THREAD_PRIORITY_VIDEO    0
# define VLC_THREAD_PRIORITY_OUTPUT  15
# define VLC_THREAD_PRIORITY_HIGHEST 20

#endif

#ifdef LIBVLC_NEED_CONDVAR
typedef struct
{
    unsigned value;
} vlc_cond_t;
# define VLC_STATIC_COND { 0 }
#endif

#ifdef LIBVLC_NEED_SEMAPHORE
typedef struct vlc_sem
{
    vlc_mutex_t lock;
    vlc_cond_t  wait;
    unsigned    value;
} vlc_sem_t;
#endif

#ifdef LIBVLC_NEED_RWLOCK
typedef struct vlc_rwlock
{
    vlc_mutex_t   mutex;
    vlc_cond_t    wait;
    long          state;
} vlc_rwlock_t;
# define VLC_STATIC_RWLOCK { VLC_STATIC_MUTEX, VLC_STATIC_COND, 0 }
#endif

/**
 * Initializes a fast mutex.
 *
 * Recursive locking of a fast mutex is undefined behaviour. (In debug builds,
 * recursive locking will cause an assertion failure.)
 */
VLC_API void vlc_mutex_init(vlc_mutex_t *);

/**
 * Initializes a recursive mutex.
 * \warning This is strongly discouraged. Please use normal mutexes.
 */
VLC_API void vlc_mutex_init_recursive(vlc_mutex_t *);

/**
 * Deinitializes a mutex.
 *
 * The mutex must not be locked, otherwise behaviour is undefined.
 */
VLC_API void vlc_mutex_destroy(vlc_mutex_t *);

/**
 * Acquires a mutex.
 *
 * If needed, this waits for any other thread to release it.
 *
 * \warning Beware of deadlocks when locking multiple mutexes at the same time,
 * or when using mutexes from callbacks.
 *
 * \note This function is not a cancellation point.
 */
VLC_API void vlc_mutex_lock(vlc_mutex_t *);

/**
 * Tries to acquire a mutex.
 *
 * This function acquires the mutex if and only if it is not currently held by
 * another thread. This function never sleeps and can be used in delay-critical
 * code paths.
 *
 * \note This function is not a cancellation point.
 *
 * \warning If this function fails, then the mutex is held... by another
 * thread. The calling thread must deal with the error appropriately. That
 * typically implies postponing the operations that would have required the
 * mutex. If the thread cannot defer those operations, then it must use
 * vlc_mutex_lock(). If in doubt, use vlc_mutex_lock() instead.
 *
 * @return 0 if the mutex could be acquired, an error code otherwise.
 */
VLC_API int vlc_mutex_trylock( vlc_mutex_t * ) VLC_USED;

/**
 * Releases a mutex.
 *
 * If the mutex is not held by the calling thread, the behaviour is undefined.
 *
 * \note This function is not a cancellation point.
 */
VLC_API void vlc_mutex_unlock(vlc_mutex_t *);

/**
 * Initializes a condition variable.
 */
VLC_API void vlc_cond_init(vlc_cond_t *);

/**
 * Initializes a condition variable (wall clock).
 *
 * This function initializes a condition variable for timed waiting using the
 * UTC wall clock time. The time reference is the same as with time() and with
 * timespec_get() and TIME_UTC.
 * vlc_cond_timedwait_daytime() must be instead of
 * vlc_cond_timedwait() for actual waiting.
 */
void vlc_cond_init_daytime(vlc_cond_t *);

/**
 * Deinitializes a condition variable.
 *
 * No threads shall be waiting or signaling the condition, otherwise the
 * behavior is undefined.
 */
VLC_API void vlc_cond_destroy(vlc_cond_t *);

/**
 * Wakes up one thread waiting on a condition variable.
 *
 * If any thread is currently waiting on the condition variable, at least one
 * of those threads will be woken up. Otherwise, this function has no effects.
 *
 * \note This function is not a cancellation point.
 */
VLC_API void vlc_cond_signal(vlc_cond_t *);

/**
 * Wakes up all threads waiting on a condition variable.
 *
 * \note This function is not a cancellation point.
 */
VLC_API void vlc_cond_broadcast(vlc_cond_t *);

/**
 * Waits on a condition variable.
 *
 * The calling thread will be suspended until another thread calls
 * vlc_cond_signal() or vlc_cond_broadcast() on the same condition variable,
 * the thread is cancelled with vlc_cancel(), or the system causes a
 * <em>spurious</em> unsolicited wake-up.
 *
 * A mutex is needed to wait on a condition variable. It must <b>not</b> be
 * a recursive mutex. Although it is possible to use the same mutex for
 * multiple condition, it is not valid to use different mutexes for the same
 * condition variable at the same time from different threads.
 *
 * The canonical way to use a condition variable to wait for event foobar is:
 @code
   vlc_mutex_lock(&lock);
   mutex_cleanup_push(&lock); // release the mutex in case of cancellation

   while (!foobar)
       vlc_cond_wait(&wait, &lock);

   // -- foobar is now true, do something about it here --

   vlc_cleanup_pop();
   vlc_mutex_unlock(&lock);
  @endcode
 *
 * \note This function is a cancellation point. In case of thread cancellation,
 * the mutex is always locked before cancellation proceeds.
 *
 * \param cond condition variable to wait on
 * \param mutex mutex which is unlocked while waiting,
 *              then locked again when waking up.
 */
VLC_API void vlc_cond_wait(vlc_cond_t *cond, vlc_mutex_t *mutex);

/**
 * Waits on a condition variable up to a certain date.
 *
 * This works like vlc_cond_wait() but with an additional time-out.
 * The time-out is expressed as an absolute timestamp using the same arbitrary
 * time reference as the mdate() and mwait() functions.
 *
 * \note This function is a cancellation point. In case of thread cancellation,
 * the mutex is always locked before cancellation proceeds.
 *
 * \param cond condition variable to wait on
 * \param mutex mutex which is unlocked while waiting,
 *              then locked again when waking up
 * \param deadline <b>absolute</b> timeout
 *
 * \warning If the variable was initialized with vlc_cond_init_daytime(), or
 * was statically initialized with \ref VLC_STATIC_COND, the time reference
 * used by this function is unspecified (depending on the implementation, it
 * might be the Unix epoch or the mdate() clock).
 *
 * \return 0 if the condition was signaled, an error code in case of timeout.
 */
VLC_API int vlc_cond_timedwait(vlc_cond_t *cond, vlc_mutex_t *mutex,
                               mtime_t deadline);

int vlc_cond_timedwait_daytime(vlc_cond_t *, vlc_mutex_t *, time_t);

/**
 * Initializes a semaphore.
 *
 * @param count initial semaphore value (typically 0)
 */
VLC_API void vlc_sem_init(vlc_sem_t *, unsigned count);

/**
 * Deinitializes a semaphore.
 */
VLC_API void vlc_sem_destroy(vlc_sem_t *);

/**
 * Increments the value of a semaphore.
 *
 * \note This function is not a cancellation point.
 *
 * \return 0 on success, EOVERFLOW in case of integer overflow.
 */
VLC_API int vlc_sem_post(vlc_sem_t *);

/**
 * Waits on a semaphore.
 *
 * This function atomically waits for the semaphore to become non-zero then
 * decrements it, and returns. If the semaphore is non-zero on entry, it is
 * immediately decremented.
 *
 * \note This function may be a point of cancellation.
 */
VLC_API void vlc_sem_wait(vlc_sem_t *);

/**
 * Initializes a read/write lock.
 */
VLC_API void vlc_rwlock_init(vlc_rwlock_t *);

/**
 * Destroys an initialized unused read/write lock.
 */
VLC_API void vlc_rwlock_destroy(vlc_rwlock_t *);

/**
 * Acquires a read/write lock for reading.
 *
 * \note Recursion is allowed.
 * \note This function may be a point of cancellation.
 */
VLC_API void vlc_rwlock_rdlock(vlc_rwlock_t *);

/**
 * Acquires a read/write lock for writing. Recursion is not allowed.
 * \note This function may be a point of cancellation.
 */
VLC_API void vlc_rwlock_wrlock(vlc_rwlock_t *);

/**
 * Releases a read/write lock.
 *
 * The calling thread must hold the lock. Otherwise behaviour is undefined.
 *
 * \note This function is not a cancellation point.
 */
VLC_API void vlc_rwlock_unlock(vlc_rwlock_t *);

/**
 * Allocates a thread-specific variable.
 *
 * @param key where to store the thread-specific variable handle
 * @param destr a destruction callback. It is called whenever a thread exits
 * and the thread-specific variable has a non-NULL value.
 *
 * @return 0 on success, a system error code otherwise.
 * This function can actually fail: on most systems, there is a fixed limit to
 * the number of thread-specific variables in a given process.
 */
VLC_API int vlc_threadvar_create(vlc_threadvar_t *key, void (*destr) (void *));

/**
 * Deallocates a thread-specific variable.
 */
VLC_API void vlc_threadvar_delete(vlc_threadvar_t *);

/**
 * Sets a thread-specific variable.

 * \param key thread-local variable key (created with vlc_threadvar_create())
 * \param value new value for the variable for the calling thread
 * \return 0 on success, a system error code otherwise.
 */
VLC_API int vlc_threadvar_set(vlc_threadvar_t key, void *value);

/**
 * Gets the value of a thread-local variable for the calling thread.
 * This function cannot fail.
 *
 * \return the value associated with the given variable for the calling
 * or NULL if no value was set.
 */
VLC_API void *vlc_threadvar_get(vlc_threadvar_t);

/**
 * Waits on an address.
 *
 * Puts the calling thread to sleep if a specific value is stored at a
 * specified address. The thread will sleep until it is woken up by a call to
 * vlc_addr_signal() or vlc_addr_broadcast() in another thread, or spuriously.
 *
 * If the value does not match, do nothing and return immediately.
 *
 * \param addr address to check for
 * \param val value to match at the address
 */
void vlc_addr_wait(void *addr, unsigned val);

/**
 * Waits on an address with a time-out.
 *
 * This function operates as vlc_addr_wait() but provides an additional
 * time-out. If the time-out elapses, the thread resumes and the function
 * returns.
 *
 * \param addr address to check for
 * \param val value to match at the address
 * \param delay time-out duration
 *
 * \return true if the function was woken up before the time-out,
 * false if the time-out elapsed.
 */
bool vlc_addr_timedwait(void *addr, unsigned val, mtime_t delay);

/**
 * Wakes up one thread on an address.
 *
 * Wakes up (at least) one of the thread sleeping on the specified address.
 * The address must be equal to the first parameter given by at least one
 * thread sleeping within the vlc_addr_wait() or vlc_addr_timedwait()
 * functions. If no threads are found, this function does nothing.
 *
 * \param addr address identifying which threads may be woken up
 */
void vlc_addr_signal(void *addr);

/**
 * Wakes up all thread on an address.
 *
 * Wakes up all threads sleeping on the specified address (if any).
 * Any thread sleeping within a call to vlc_addr_wait() or vlc_addr_timedwait()
 * with the specified address as first call parameter will be woken up.
 *
 * \param addr address identifying which threads to wake up
 */
void vlc_addr_broadcast(void *addr);

/**
 * Creates and starts a new thread.
 *
 * The thread must be <i>joined</i> with vlc_join() to reclaim resources
 * when it is not needed anymore.
 *
 * @param th storage space for the handle of the new thread (cannot be NULL)
 *           [OUT]
 * @param entry entry point for the thread
 * @param data data parameter given to the entry point
 * @param priority thread priority value
 * @return 0 on success, a standard error code on error.
 * @note In case of error, the value of *th is undefined.
 */
VLC_API int vlc_clone(vlc_thread_t *th, void *(*entry)(void *), void *data,
                      int priority) VLC_USED;

/**
 * Marks a thread as cancelled.
 *
 * Next time the target thread reaches a cancellation point (while not having
 * disabled cancellation), it will run its cancellation cleanup handler, the
 * thread variable destructors, and terminate.
 *
 * vlc_join() must be used regardless of a thread being cancelled or not, to
 * avoid leaking resources.
 */
VLC_API void vlc_cancel(vlc_thread_t);

/**
 * Waits for a thread to complete (if needed), then destroys it.
 *
 * \note This is a cancellation point. In case of cancellation, the thread is
 * <b>not</b> joined.

 * \warning A thread cannot join itself (normally VLC will abort if this is
 * attempted). Also a detached thread <b>cannot</b> be joined.
 *
 * @param th thread handle
 * @param result [OUT] pointer to write the thread return value or NULL
 */
VLC_API void vlc_join(vlc_thread_t th, void **result);

/**
 * Disables thread cancellation.
 *
 * This functions saves the current cancellation state (enabled or disabled),
 * then disables cancellation for the calling thread. It must be called before
 * entering a piece of code that is not cancellation-safe, unless it can be
 * proven that the calling thread will not be cancelled.
 *
 * \note This function is not a cancellation point.
 *
 * \return Previous cancellation state (opaque value for vlc_restorecancel()).
 */
VLC_API int vlc_savecancel(void);

/**
 * Restores the cancellation state.
 *
 * This function restores the cancellation state of the calling thread to
 * a state previously saved by vlc_savecancel().
 *
 * \note This function is not a cancellation point.
 *
 * \param state previous state as returned by vlc_savecancel().
 */
VLC_API void vlc_restorecancel(int state);

/**
 * Internal handler for thread cancellation.
 *
 * Do not call this function directly. Use wrapper macros instead:
 * vlc_cleanup_push(), vlc_cleanup_pop().
 */
VLC_API void vlc_control_cancel(int cmd, ...);

/**
 * Thread handle.
 *
 * This function returns the thread handle of the calling thread.
 *
 * \note The exact type of the thread handle depends on the platform,
 * including an integer type, a pointer type or a compound type of any size.
 * If you need an integer identifier, use vlc_thread_id() instead.
 *
 * \note vlc_join(vlc_thread_self(), NULL) is undefined,
 * as it obviously does not make any sense (it might result in a deadlock, but
 * there are no warranties that it will).
 *
 * \return the thread handle
 */
VLC_API vlc_thread_t vlc_thread_self(void) VLC_USED;

/**
 * Thread identifier.
 *
 * This function returns the identifier of the calling thread. The identifier
 * cannot change for the entire duration of the thread, and no other thread can
 * have the same identifier at the same time in the same process. Typically,
 * the identifier is also unique across all running threads of all existing
 * processes, but that depends on the operating system.
 *
 * There are no particular semantics to the thread ID with LibVLC.
 * It is provided mainly for tracing and debugging.
 *
 * \warning This function is not currently implemented on all supported
 * platforms. Where not implemented, it returns (unsigned long)-1.
 *
 * \return the thread identifier (or -1 if unimplemented)
 */
VLC_API unsigned long vlc_thread_id(void) VLC_USED;

/**
 * Precision monotonic clock.
 *
 * In principles, the clock has a precision of 1 MHz. But the actual resolution
 * may be much lower, especially when it comes to sleeping with mwait() or
 * msleep(). Most general-purpose operating systems provide a resolution of
 * only 100 to 1000 Hz.
 *
 * \warning The origin date (time value "zero") is not specified. It is
 * typically the time the kernel started, but this is platform-dependent.
 * If you need wall clock time, use gettimeofday() instead.
 *
 * \return a timestamp in microseconds.
 */
VLC_API mtime_t mdate(void);

/**
 * Waits until a deadline.
 *
 * \param deadline timestamp to wait for (\ref mdate())
 *
 * \note The deadline may be exceeded due to OS scheduling.
 * \note This function is a cancellation point.
 */
VLC_API void mwait(mtime_t deadline);

/**
 * Waits for an interval of time.
 *
 * \param delay how long to wait (in microseconds)
 *
 * \note The delay may be exceeded due to OS scheduling.
 * \note This function is a cancellation point.
 */
VLC_API void msleep(mtime_t delay);

#define VLC_HARD_MIN_SLEEP   10000 /* 10 milliseconds = 1 tick at 100Hz */
#define VLC_SOFT_MIN_SLEEP 9000000 /* 9 seconds */

#if defined (__GNUC__) && !defined (__clang__)
/* Linux has 100, 250, 300 or 1000Hz
 *
 * HZ=100 by default on FreeBSD, but some architectures use a 1000Hz timer
 */

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((error("sorry, cannot sleep for such short a time")))
mtime_t impossible_delay( mtime_t delay )
{
    (void) delay;
    return VLC_HARD_MIN_SLEEP;
}

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((warning("use proper event handling instead of short delay")))
mtime_t harmful_delay( mtime_t delay )
{
    return delay;
}

# define check_delay( d ) \
    ((__builtin_constant_p(d < VLC_HARD_MIN_SLEEP) \
   && (d < VLC_HARD_MIN_SLEEP)) \
       ? impossible_delay(d) \
       : ((__builtin_constant_p(d < VLC_SOFT_MIN_SLEEP) \
       && (d < VLC_SOFT_MIN_SLEEP)) \
           ? harmful_delay(d) \
           : d))

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((error("deadlines can not be constant")))
mtime_t impossible_deadline( mtime_t deadline )
{
    return deadline;
}

# define check_deadline( d ) \
    (__builtin_constant_p(d) ? impossible_deadline(d) : d)
#else
# define check_delay(d) (d)
# define check_deadline(d) (d)
#endif

#define msleep(d) msleep(check_delay(d))
#define mwait(d) mwait(check_deadline(d))

/**
 * Initializes an asynchronous timer.
 *
 * \param id pointer to timer to be initialized
 * \param func function that the timer will call
 * \param data parameter for the timer function
 * \return 0 on success, a system error code otherwise.
 *
 * \warning Asynchronous timers are processed from an unspecified thread.
 * \note Multiple occurrences of a single interval timer are serialized:
 * they cannot run concurrently.
 */
VLC_API int vlc_timer_create(vlc_timer_t *id, void (*func)(void *), void *data)
VLC_USED;

/**
 * Destroys an initialized timer.
 *
 * If needed, the timer is first disarmed. Behaviour is undefined if the
 * specified timer is not initialized.
 *
 * \warning This function <b>must</b> be called before the timer data can be
 * freed and before the timer callback function can be unmapped/unloaded.
 *
 * \param timer timer to destroy
 */
VLC_API void vlc_timer_destroy(vlc_timer_t timer);

/**
 * Arms or disarms an initialized timer.
 *
 * This functions overrides any previous call to itself.
 *
 * \note A timer can fire later than requested due to system scheduling
 * limitations. An interval timer can fail to trigger sometimes, either because
 * the system is busy or suspended, or because a previous iteration of the
 * timer is still running. See also vlc_timer_getoverrun().
 *
 * \param timer initialized timer
 * \param absolute the timer value origin is the same as mdate() if true,
 *                 the timer value is relative to now if false.
 * \param value zero to disarm the timer, otherwise the initial time to wait
 *              before firing the timer.
 * \param interval zero to fire the timer just once, otherwise the timer
 *                 repetition interval.
 */
VLC_API void vlc_timer_schedule(vlc_timer_t timer, bool absolute,
                                mtime_t value, mtime_t interval);

/**
 * Fetches and resets the overrun counter for a timer.
 *
 * This functions returns the number of times that the interval timer should
 * have fired, but the callback was not invoked due to scheduling problems.
 * The call resets the counter to zero.
 *
 * \param timer initialized timer
 * \return the timer overrun counter (typically zero)
 */
VLC_API unsigned vlc_timer_getoverrun(vlc_timer_t) VLC_USED;

/**
 * Count CPUs.
 *
 * \return number of available (logical) CPUs.
 */
VLC_API unsigned vlc_GetCPUCount(void);

enum
{
    VLC_CLEANUP_PUSH,
    VLC_CLEANUP_POP,
    VLC_CANCEL_ADDR_SET,
    VLC_CANCEL_ADDR_CLEAR,
};

#if defined (LIBVLC_USE_PTHREAD_CLEANUP)
/**
 * Registers a thread cancellation handler.
 *
 * This pushes a function to run if the thread is cancelled (or otherwise
 * exits prematurely).
 *
 * If multiple procedures are registered,
 * they are handled in last-in first-out order.
 *
 * \note Any call to vlc_cleanup_push() <b>must</b> paired with a call to
 * vlc_cleanup_pop().
 * \warning Branching into or out of the block between these two function calls
 * is not allowed (read: it will likely crash the whole process).
 *
 * \param routine procedure to call if the thread ends
 * \param arg argument for the procedure
 */
# define vlc_cleanup_push( routine, arg ) pthread_cleanup_push (routine, arg)

/**
 * Unregisters the last cancellation handler.
 *
 * This pops the cancellation handler that was last pushed with
 * vlc_cleanup_push() in the calling thread.
 */
# define vlc_cleanup_pop( ) pthread_cleanup_pop (0)

#else
typedef struct vlc_cleanup_t vlc_cleanup_t;

struct vlc_cleanup_t
{
    vlc_cleanup_t *next;
    void         (*proc) (void *);
    void          *data;
};

/* This macros opens a code block on purpose. This is needed for multiple
 * calls within a single function. This also prevent Win32 developers from
 * writing code that would break on POSIX (POSIX opens a block as well). */
# define vlc_cleanup_push( routine, arg ) \
    do { \
        vlc_cleanup_t vlc_cleanup_data = { NULL, routine, arg, }; \
        vlc_control_cancel (VLC_CLEANUP_PUSH, &vlc_cleanup_data)

# define vlc_cleanup_pop( ) \
        vlc_control_cancel (VLC_CLEANUP_POP); \
    } while (0)

#endif /* !LIBVLC_USE_PTHREAD_CLEANUP */

static inline void vlc_cleanup_lock (void *lock)
{
    vlc_mutex_unlock ((vlc_mutex_t *)lock);
}
#define mutex_cleanup_push( lock ) vlc_cleanup_push (vlc_cleanup_lock, lock)

static inline void vlc_cancel_addr_set(void *addr)
{
    vlc_control_cancel(VLC_CANCEL_ADDR_SET, addr);
}

static inline void vlc_cancel_addr_clear(void *addr)
{
    vlc_control_cancel(VLC_CANCEL_ADDR_CLEAR, addr);
}

#ifdef __cplusplus
/**
 * Helper C++ class to lock a mutex.
 *
 * The mutex is locked when the object is created, and unlocked when the object
 * is destroyed.
 */
class vlc_mutex_locker
{
    private:
        vlc_mutex_t *lock;
    public:
        vlc_mutex_locker (vlc_mutex_t *m) : lock (m)
        {
            vlc_mutex_lock (lock);
        }

        ~vlc_mutex_locker (void)
        {
            vlc_mutex_unlock (lock);
        }
};
#endif

enum
{
   VLC_AVCODEC_MUTEX = 0,
   VLC_GCRYPT_MUTEX,
   VLC_XLIB_MUTEX,
   VLC_MOSAIC_MUTEX,
   VLC_HIGHLIGHT_MUTEX,
#ifdef _WIN32
   VLC_MTA_MUTEX,
#endif
   /* Insert new entry HERE */
   VLC_MAX_MUTEX
};

/**
 * Internal handler for global mutexes.
 *
 * Do not use this function directly. Use helper macros instead:
 * vlc_global_lock(), vlc_global_unlock().
 */
VLC_API void vlc_global_mutex(unsigned, bool);

/**
 * Acquires a global mutex.
 */
#define vlc_global_lock( n ) vlc_global_mutex(n, true)

/**
 * Releases a global mutex.
 */
#define vlc_global_unlock( n ) vlc_global_mutex(n, false)

/** @} */

#endif /* !_VLC_THREADS_H */
