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

#ifndef __cplusplus
#include <stdatomic.h>
#endif

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

typedef struct vlc_threadvar *vlc_threadvar_t;
typedef struct vlc_timer *vlc_timer_t;

# define VLC_THREAD_PRIORITY_LOW      0
# define VLC_THREAD_PRIORITY_INPUT    1
# define VLC_THREAD_PRIORITY_AUDIO    VLC_THREAD_PRIORITY_HIGHEST
# define VLC_THREAD_PRIORITY_VIDEO    0
# define VLC_THREAD_PRIORITY_OUTPUT   1
# define VLC_THREAD_PRIORITY_HIGHEST  2

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

typedef struct vlc_thread *vlc_thread_t;
#define VLC_THREAD_CANCELED NULL
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

#else /* POSIX threads */
# include <unistd.h> /* _POSIX_SPIN_LOCKS */
# include <pthread.h>

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
 * Thread-local key handle.
 *
 * \ingroup threadvar
 */
typedef pthread_key_t   vlc_threadvar_t;

/**
 * Threaded timer handle.
 *
 * \ingroup timer
 */
typedef struct vlc_timer *vlc_timer_t;

/* Thread priorities.
 * No effect for POSIX threads
 */
# define VLC_THREAD_PRIORITY_LOW      0
# define VLC_THREAD_PRIORITY_INPUT    0
# define VLC_THREAD_PRIORITY_AUDIO    0
# define VLC_THREAD_PRIORITY_VIDEO    0
# define VLC_THREAD_PRIORITY_OUTPUT   0
# define VLC_THREAD_PRIORITY_HIGHEST  0

#endif

/**
 * \defgroup mutex Mutual exclusion locks
 * @{
 */
/**
 * Mutex.
 *
 * Storage space for a mutual exclusion lock.
 */
typedef struct
{
    union {
#ifndef __cplusplus
        struct {
            atomic_uint value;
            atomic_uint recursion;
            atomic_ulong owner;
        };
#endif
        struct {
            unsigned int value;
            unsigned int recursion;
            unsigned long owner;
        } dummy;
    };
} vlc_mutex_t;

/**
 * Static initializer for (static) mutex.
 *
 * \note This only works in C code.
 * In C++, consider using a global \ref vlc::threads::mutex instance instead.
 */
#define VLC_STATIC_MUTEX { \
    .value = ATOMIC_VAR_INIT(0), \
    .recursion = ATOMIC_VAR_INIT(0), \
    .owner = ATOMIC_VAR_INIT(0), \
}

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
 * Checks if a mutex is locked.
 *
 * Do not use this function directly. Use vlc_mutex_assert() instead.
 *
 * @note
 * This function has no effects.
 * It is only meant to be use in run-time assertions.
 *
 * @warning This function might not return correct results in non-debug builds.
 *
 * @retval false the mutex is not locked by the calling thread
 * @retval true the mutex is locked by the calling thread
 */
VLC_API bool vlc_mutex_held(const vlc_mutex_t *) VLC_USED;

/**
 * Asserts that a mutex is locked by the calling thread.
 */
#define vlc_mutex_assert(m) assert(vlc_mutex_held(m))

/** @} */

/**
 * \defgroup condvar Condition variables
 *
 * The condition variable is the most common and generic mean for threads to
 * wait for events triggered by other threads.
 *
 * See also POSIX @c pthread_cond_t .
 * @{
 */

struct vlc_cond_waiter;

/**
 * Condition variable.
 *
 * Storage space for a thread condition variable.
 */
typedef struct
{
    struct vlc_cond_waiter *head;
    vlc_mutex_t lock;
} vlc_cond_t;

/**
 * Static initializer for (static) condition variable.
 */
#define VLC_STATIC_COND { NULL, VLC_STATIC_MUTEX }

/**
 * Initializes a condition variable.
 */
VLC_API void vlc_cond_init(vlc_cond_t *);

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

   while (!foobar)
       vlc_cond_wait(&wait, &lock);

   // -- foobar is now true, do something about it here --

   vlc_mutex_unlock(&lock);
  @endcode
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
 * time reference as the vlc_tick_now() and vlc_tick_wait() functions.
 *
 * \param cond condition variable to wait on
 * \param mutex mutex which is unlocked while waiting,
 *              then locked again when waking up
 * \param deadline <b>absolute</b> timeout
 *
 * \return 0 if the condition was signaled, an error code in case of timeout.
 */
VLC_API int vlc_cond_timedwait(vlc_cond_t *cond, vlc_mutex_t *mutex,
                               vlc_tick_t deadline);

int vlc_cond_timedwait_daytime(vlc_cond_t *, vlc_mutex_t *, time_t);

/** @} */

/**
 * \defgroup semaphore Semaphores
 *
 * The semaphore is the simplest thread synchronization primitive, consisting
 * of a simple counter.
 *
 * See also POSIX @c sem_t .
 *
 * @{
 */
/**
 * Semaphore.
 *
 * Storage space for a thread-safe semaphore.
 */
typedef struct
{
    union {
#ifndef __cplusplus
        atomic_uint value;
#endif
        int dummy;
   };
} vlc_sem_t;

/**
 * Initializes a semaphore.
 *
 * @param count initial semaphore value (typically 0)
 */
VLC_API void vlc_sem_init(vlc_sem_t *, unsigned count);

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
 * Tries to decrement a semaphore.
 *
 * This function decrements the semaphore if its value is not zero.
 *
 * \param sem semaphore to decrement
 *
 * \retval 0 the semaphore was decremented
 * \retval EAGAIN the semaphore was zero and could not be decremented
 */
VLC_API int vlc_sem_trywait(vlc_sem_t *sem) VLC_USED;

/**
 * Waits on a semaphore within a deadline.
 *
 * This function waits for the semaphore just like vlc_sem_wait(), but only
 * up to a given deadline.
 *
 * \param sem semaphore to wait for
 * \param deadline deadline to wait until
 *
 * \retval 0 the semaphore was decremented
 * \retval ETIMEDOUT the deadline was reached
 */
VLC_API int vlc_sem_timedwait(vlc_sem_t *sem, vlc_tick_t deadline) VLC_USED;

/** @} */

/**
 * \defgroup rwlock Read/write locks
 *
 * Read/write locks are a type of thread synchronization primitive meant to
 * protect access to data that is mostly read, and rarely written.
 * As long as no threads tries to acquire the lock for "writing", any number of
 * threads can acquire the lock for "reading".
 *
 * See also POSIX @c pthread_rwlock_t .
 *
 * @{
 */

/**
 * Read/write lock.
 *
 * Storage space for a slim reader/writer lock.
 */
typedef struct vlc_rwlock
{
    vlc_mutex_t   mutex;
    vlc_cond_t    wait;
    long          state;
} vlc_rwlock_t;

/**
 * Static initializer for (static) read/write lock.
 */
#define VLC_STATIC_RWLOCK { VLC_STATIC_MUTEX, VLC_STATIC_COND, 0 }

/**
 * Initializes a read/write lock.
 */
VLC_API void vlc_rwlock_init(vlc_rwlock_t *);

/**
 * Acquires a read/write lock for reading.
 *
 * \note Recursion is allowed.
 */
VLC_API void vlc_rwlock_rdlock(vlc_rwlock_t *);

/**
 * Acquires a read/write lock for writing. Recursion is not allowed.
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

/** @} */

#ifndef __cplusplus
/**
 * One-time initialization.
 *
 * A one-time initialization object must always be initialized assigned to
 * \ref VLC_STATIC_ONCE before use.
 */
typedef struct
{
    atomic_uint value;
} vlc_once_t;

/**
 * Static initializer for one-time initialization.
 */
#define VLC_STATIC_ONCE { ATOMIC_VAR_INIT(0) }

/**
 * Executes a function one time.
 *
 * The first time this function is called with a given one-time initialization
 * object, it executes the provided callback.
 * Any further call with the same object will be a no-op.
 *
 * In the corner case that the first time execution is ongoing in another
 * thread, then the function will wait for completion on the other thread
 * (and then synchronize memory) before it returns.
 * This ensures that, no matter what, the callback has been executed exactly
 * once and its side effects are visible after the function returns.
 *
 * \param once a one-time initialization object
 * \param cb callback to execute (the first time)
 */
VLC_API void vlc_once(vlc_once_t *restrict once, void (*cb)(void));

static inline void vlc_once_inline(vlc_once_t *restrict once, void (*cb)(void))
{
    /* Fast path: check if already initialized */
    if (unlikely(atomic_load_explicit(&once->value, memory_order_acquire) < 3))
        vlc_once(once, cb);
}
#define vlc_once(once, cb) vlc_once_inline(once, cb)
#endif

/**
 * \defgroup threadvar Thread-specific variables
 * @{
 */
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

/** @} */

/**
 * Waits on an address.
 *
 * Puts the calling thread to sleep if a specific unsigned 32-bits value is
 * stored at a specified address. The thread will sleep until it is woken up by
 * a call to vlc_atomic_notify_one() or vlc_atomic_notify_all() in another
 * thread, or spuriously.
 *
 * If the value does not match, do nothing and return immediately.
 *
 * \param addr address to check for
 * \param val value to match at the address
 */
void vlc_atomic_wait(void *addr, unsigned val);

/**
 * Waits on an address with a time-out.
 *
 * This function operates as vlc_atomic_wait() but provides an additional
 * time-out. If the deadline is reached, the thread resumes and the function
 * returns.
 *
 * \param addr address to check for
 * \param val value to match at the address
 * \param deadline deadline to wait until
 *
 * \retval 0 the function was woken up before the time-out
 * \retval ETIMEDOUT the deadline was reached
 */
int vlc_atomic_timedwait(void *addr, unsigned val, vlc_tick_t deadline);

int vlc_atomic_timedwait_daytime(void *addr, unsigned val, time_t deadline);

/**
 * Wakes up one thread on an address.
 *
 * Wakes up (at least) one of the thread sleeping on the specified address.
 * The address must be equal to the first parameter given by at least one
 * thread sleeping within the vlc_atomic_wait() or vlc_atomic_timedwait()
 * functions. If no threads are found, this function does nothing.
 *
 * \param addr address identifying which threads may be woken up
 */
void vlc_atomic_notify_one(void *addr);

/**
 * Wakes up all thread on an address.
 *
 * Wakes up all threads sleeping on the specified address (if any).
 * Any thread sleeping within a call to vlc_atomic_wait() or
 * vlc_atomic_timedwait() with the specified address as first call parameter
 * will be woken up.
 *
 * \param addr address identifying which threads to wake up
 */
void vlc_atomic_notify_all(void *addr);

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
 * Set the thread name of the current thread.
 *
 * \param name the string to use as the thread name
 *
 * \note On Linux the name can be up to 16 bytes long, including the terminating
 * null byte on Linux. If larger the name will be truncated.
 */
VLC_API void vlc_thread_set_name(const char *name);

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

typedef struct vlc_cleanup_t vlc_cleanup_t;

/**
 * Internal handler for thread cancellation.
 *
 * Do not call this function directly. Use wrapper macros instead:
 * vlc_cleanup_push(), vlc_cleanup_pop().
 */
VLC_API void vlc_control_cancel(vlc_cleanup_t *);

/**
 * Thread identifier.
 *
 * This function returns a non-zero unique identifier of the calling thread.
 * The identifier cannot change for the entire lifetime of the thread, and two
 * concurrent threads cannot have the same identifier.
 *
 * The thread identifier has no defined semantics other than uniqueness,
 * and no particular purposes within LibVLC.
 * It is provided mainly for tracing and debugging.
 *
 * On some but not all supported platforms, the thread identifier is in fact
 * the OS/kernel thread identifier (a.k.a. task PID), and is temporally unique
 * not only within the process but across the entire system.
 *
 * \note
 * The `main()` thread identifier is typically identical to the process
 * identifier, but this is not portable.
 *
 * \return the thread identifier (cannot fail)
 */
VLC_API unsigned long vlc_thread_id(void) VLC_USED;

/**
 * Precision monotonic clock.
 *
 * In principles, the clock has a precision of 1 MHz. But the actual resolution
 * may be much lower, especially when it comes to sleeping with vlc_tick_wait() or
 * vlc_tick_sleep(). Most general-purpose operating systems provide a resolution of
 * only 100 to 1000 Hz.
 *
 * \warning The origin date (time value "zero") is not specified. It is
 * typically the time the kernel started, but this is platform-dependent.
 * If you need wall clock time, use gettimeofday() instead.
 *
 * \return a timestamp in microseconds.
 */
VLC_API vlc_tick_t vlc_tick_now(void);

/**
 * Waits until a deadline.
 *
 * \param deadline timestamp to wait for (\ref vlc_tick_now())
 *
 * \note The deadline may be exceeded due to OS scheduling.
 * \note This function is a cancellation point.
 */
VLC_API void vlc_tick_wait(vlc_tick_t deadline);

/**
 * Waits for an interval of time.
 *
 * \param delay how long to wait (in microseconds)
 *
 * \note The delay may be exceeded due to OS scheduling.
 * \note This function is a cancellation point.
 */
VLC_API void vlc_tick_sleep(vlc_tick_t delay);

#define VLC_HARD_MIN_SLEEP  VLC_TICK_FROM_MS(10)   /* 10 milliseconds = 1 tick at 100Hz */
#define VLC_SOFT_MIN_SLEEP  VLC_TICK_FROM_SEC(9)   /* 9 seconds */

#if defined (__GNUC__) && !defined (__clang__)
/* Linux has 100, 250, 300 or 1000Hz
 *
 * HZ=100 by default on FreeBSD, but some architectures use a 1000Hz timer
 */

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((error("sorry, cannot sleep for such short a time")))
vlc_tick_t impossible_delay( vlc_tick_t delay )
{
    (void) delay;
    return VLC_HARD_MIN_SLEEP;
}

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((warning("use proper event handling instead of short delay")))
vlc_tick_t harmful_delay( vlc_tick_t delay )
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
vlc_tick_t impossible_deadline( vlc_tick_t deadline )
{
    return deadline;
}

# define check_deadline( d ) \
    (__builtin_constant_p(d) ? impossible_deadline(d) : d)
#else
# define check_delay(d) (d)
# define check_deadline(d) (d)
#endif

#define vlc_tick_sleep(d) vlc_tick_sleep(check_delay(d))
#define vlc_tick_wait(d) vlc_tick_wait(check_deadline(d))

/**
 * \defgroup timer Asynchronous/threaded timers
 * @{
 */
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

#define VLC_TIMER_DISARM    (0)
#define VLC_TIMER_FIRE_ONCE (0)

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
 * \param absolute the timer value origin is the same as vlc_tick_now() if true,
 *                 the timer value is relative to now if false.
 * \param value zero to disarm the timer, otherwise the initial time to wait
 *              before firing the timer.
 * \param interval zero to fire the timer just once, otherwise the timer
 *                 repetition interval.
 */
VLC_API void vlc_timer_schedule(vlc_timer_t timer, bool absolute,
                                vlc_tick_t value, vlc_tick_t interval);

static inline void vlc_timer_disarm(vlc_timer_t timer)
{
    vlc_timer_schedule( timer, false, VLC_TIMER_DISARM, 0 );
}

static inline void vlc_timer_schedule_asap(vlc_timer_t timer, vlc_tick_t interval)
{
    vlc_timer_schedule(timer, false, 1, interval);
}

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

/** @} */

/**
 * Count CPUs.
 *
 * \return number of available (logical) CPUs.
 */
VLC_API unsigned vlc_GetCPUCount(void);

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

#else /* !LIBVLC_USE_PTHREAD_CLEANUP */
struct vlc_cleanup_t
{
    vlc_cleanup_t *next;
    void         (*proc) (void *);
    void          *data;
};

# ifndef __cplusplus
/* This macros opens a code block on purpose: It reduces the chance of
 * not pairing the push and pop. It also matches the POSIX Thread internals.
 * That way, Win32 developers will not accidentally break other platforms.
 */
# define vlc_cleanup_push( routine, arg ) \
    do { \
        vlc_control_cancel(&(vlc_cleanup_t){ NULL, routine, arg })

#  define vlc_cleanup_pop( ) \
        vlc_control_cancel (NULL); \
    } while (0)
# else
#  define vlc_cleanup_push(routine, arg) \
    static_assert(false, "don't use vlc_cleanup_push in portable C++ code")
#  define vlc_cleanup_pop() \
    static_assert(false, "don't use vlc_cleanup_pop in portable C++ code")
# endif

#endif /* !LIBVLC_USE_PTHREAD_CLEANUP */

static inline void vlc_cleanup_lock (void *lock)
{
    vlc_mutex_unlock ((vlc_mutex_t *)lock);
}
#define mutex_cleanup_push( lock ) vlc_cleanup_push (vlc_cleanup_lock, lock)

#ifndef __cplusplus
void vlc_cancel_addr_set(atomic_uint *addr);
void vlc_cancel_addr_clear(atomic_uint *addr);
#else
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
