/*****************************************************************************
 * threads.c: LibVLC generic thread support
 *****************************************************************************
 * Copyright (C) 2009-2016 Rémi Denis-Courmont
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

#include <assert.h>
#include <errno.h>

#include <vlc_common.h>

/*** Global locks ***/

void vlc_global_mutex (unsigned n, bool acquire)
{
    static vlc_mutex_t locks[] = {
        VLC_STATIC_MUTEX,
        VLC_STATIC_MUTEX,
        VLC_STATIC_MUTEX,
        VLC_STATIC_MUTEX,
        VLC_STATIC_MUTEX,
#ifdef _WIN32
        VLC_STATIC_MUTEX, // For MTA holder
#endif
    };
    static_assert (VLC_MAX_MUTEX == (sizeof (locks) / sizeof (locks[0])),
                   "Wrong number of global mutexes");
    assert (n < (sizeof (locks) / sizeof (locks[0])));

    vlc_mutex_t *lock = locks + n;
    if (acquire)
        vlc_mutex_lock (lock);
    else
        vlc_mutex_unlock (lock);
}

#if defined (_WIN32) && (_WIN32_WINNT < _WIN32_WINNT_WIN8)
/* Cannot define OS version-dependent stuff in public headers */
# undef LIBVLC_NEED_SLEEP
# undef LIBVLC_NEED_SEMAPHORE
#endif

#if defined(LIBVLC_NEED_SLEEP) || defined(LIBVLC_NEED_CONDVAR)
#include <vlc_atomic.h>

static void vlc_cancel_addr_prepare(void *addr)
{
    /* Let thread subsystem on address to broadcast for cancellation */
    vlc_cancel_addr_set(addr);
    vlc_cleanup_push(vlc_cancel_addr_clear, addr);
    /* Check if cancellation was pending before vlc_cancel_addr_set() */
    vlc_testcancel();
    vlc_cleanup_pop();
}

static void vlc_cancel_addr_finish(void *addr)
{
    vlc_cancel_addr_clear(addr);
    /* Act on cancellation as potential wake-up source */
    vlc_testcancel();
}
#endif

#ifdef LIBVLC_NEED_SLEEP
void (mwait)(mtime_t deadline)
{
    mtime_t delay;
    atomic_int value = ATOMIC_VAR_INIT(0);

    vlc_cancel_addr_prepare(&value);

    while ((delay = (deadline - mdate())) > 0)
    {
        vlc_addr_timedwait(&value, 0, delay);
        vlc_testcancel();
    }

    vlc_cancel_addr_finish(&value);
}

void (msleep)(mtime_t delay)
{
    mwait(mdate() + delay);
}
#endif

#ifdef LIBVLC_NEED_CONDVAR
#include <stdalign.h>

static inline atomic_uint *vlc_cond_value(vlc_cond_t *cond)
{
    /* XXX: ugly but avoids including vlc_atomic.h in vlc_threads.h */
    static_assert (sizeof (cond->value) <= sizeof (atomic_uint),
                   "Size mismatch!");
    static_assert ((alignof (cond->value) % alignof (atomic_uint)) == 0,
                   "Alignment mismatch");
    return (atomic_uint *)&cond->value;
}

void vlc_cond_init(vlc_cond_t *cond)
{
    /* Initial value is irrelevant but set it for happy debuggers */
    atomic_init(vlc_cond_value(cond), 0);
}

void vlc_cond_init_daytime(vlc_cond_t *cond)
{
    vlc_cond_init(cond);
}

void vlc_cond_destroy(vlc_cond_t *cond)
{
    /* Tempting sanity check but actually incorrect:
    assert((atomic_load_explicit(vlc_cond_value(cond),
                                 memory_order_relaxed) & 1) == 0);
     * Due to timeouts and spurious wake-ups, the futex value can look like
     * there are waiters, even though there are none. */
    (void) cond;
}

void vlc_cond_signal(vlc_cond_t *cond)
{
    /* Probably the best documented approach is that of Bionic: increment
     * the futex here, and simply load the value in cnd_wait(). This has a bug
     * as unlikely as well-known: signals get lost if the futex is incremented
     * an exact multiple of 2^(CHAR_BIT * sizeof (int)) times.
     *
     * A different presumably bug-free solution is used here:
     * - cnd_signal() sets the futex to the equal-or-next odd value, while
     * - cnd_wait() sets the futex to the equal-or-next even value.
     **/
    atomic_fetch_or_explicit(vlc_cond_value(cond), 1, memory_order_relaxed);
    vlc_addr_signal(&cond->value);
}

void vlc_cond_broadcast(vlc_cond_t *cond)
{
    atomic_fetch_or_explicit(vlc_cond_value(cond), 1, memory_order_relaxed);
    vlc_addr_broadcast(&cond->value);
}

void vlc_cond_wait(vlc_cond_t *cond, vlc_mutex_t *mutex)
{
    unsigned value = atomic_load_explicit(vlc_cond_value(cond),
                                     memory_order_relaxed);
    while (value & 1)
    {
        if (atomic_compare_exchange_weak_explicit(vlc_cond_value(cond), &value,
                                                  value + 1,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed))
            value++;
    }

    vlc_cancel_addr_prepare(&cond->value);
    vlc_mutex_unlock(mutex);

    vlc_addr_wait(&cond->value, value);

    vlc_mutex_lock(mutex);
    vlc_cancel_addr_finish(&cond->value);
}

static int vlc_cond_wait_delay(vlc_cond_t *cond, vlc_mutex_t *mutex,
                               mtime_t delay)
{
    unsigned value = atomic_load_explicit(vlc_cond_value(cond),
                                          memory_order_relaxed);
    while (value & 1)
    {
        if (atomic_compare_exchange_weak_explicit(vlc_cond_value(cond), &value,
                                                  value + 1,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed))
            value++;
    }

    vlc_cancel_addr_prepare(&cond->value);
    vlc_mutex_unlock(mutex);

    if (delay > 0)
        value = vlc_addr_timedwait(&cond->value, value, delay);
    else
        value = 0;

    vlc_mutex_lock(mutex);
    vlc_cancel_addr_finish(&cond->value);

    return value ? 0 : ETIMEDOUT;
}

int vlc_cond_timedwait(vlc_cond_t *cond, vlc_mutex_t *mutex, mtime_t deadline)
{
    return vlc_cond_wait_delay(cond, mutex, deadline - mdate());
}

int vlc_cond_timedwait_daytime(vlc_cond_t *cond, vlc_mutex_t *mutex,
                               time_t deadline)
{
    struct timespec ts;

    timespec_get(&ts, TIME_UTC);
    deadline -= ts.tv_sec * CLOCK_FREQ;
    deadline -= ts.tv_nsec / (1000000000 / CLOCK_FREQ);

    return vlc_cond_wait_delay(cond, mutex, deadline);
}
#endif

#ifdef LIBVLC_NEED_RWLOCK
/*** Generic read/write locks ***/
#include <stdlib.h>
#include <limits.h>
/* NOTE:
 * lock->state is a signed long integer:
 *  - The sign bit is set when the lock is held for writing.
 *  - The other bits code the number of times the lock is held for reading.
 * Consequently:
 *  - The value is negative if and only if the lock is held for writing.
 *  - The value is zero if and only if the lock is not held at all.
 */
#define READER_MASK LONG_MAX
#define WRITER_BIT  LONG_MIN

void vlc_rwlock_init (vlc_rwlock_t *lock)
{
    vlc_mutex_init (&lock->mutex);
    vlc_cond_init (&lock->wait);
    lock->state = 0;
}

void vlc_rwlock_destroy (vlc_rwlock_t *lock)
{
    vlc_cond_destroy (&lock->wait);
    vlc_mutex_destroy (&lock->mutex);
}

void vlc_rwlock_rdlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    /* Recursive read-locking is allowed.
     * Ensure that there is no active writer. */
    while (lock->state < 0)
    {
        assert (lock->state == WRITER_BIT);
        mutex_cleanup_push (&lock->mutex);
        vlc_cond_wait (&lock->wait, &lock->mutex);
        vlc_cleanup_pop ();
    }
    if (unlikely(lock->state >= READER_MASK))
        abort (); /* An overflow is certainly a recursion bug. */
    lock->state++;
    vlc_mutex_unlock (&lock->mutex);
}

void vlc_rwlock_wrlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    /* Wait until nobody owns the lock in any way. */
    while (lock->state != 0)
    {
        mutex_cleanup_push (&lock->mutex);
        vlc_cond_wait (&lock->wait, &lock->mutex);
        vlc_cleanup_pop ();
    }
    lock->state = WRITER_BIT;
    vlc_mutex_unlock (&lock->mutex);
}

void vlc_rwlock_unlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    if (lock->state < 0)
    {   /* Write unlock */
        assert (lock->state == WRITER_BIT);
        /* Let reader and writer compete. OS scheduler decides who wins. */
        lock->state = 0;
        vlc_cond_broadcast (&lock->wait);
    }
    else
    {   /* Read unlock */
        assert (lock->state > 0);
        /* If there are no readers left, wake up one pending writer. */
        if (--lock->state == 0)
            vlc_cond_signal (&lock->wait);
    }
    vlc_mutex_unlock (&lock->mutex);
}
#endif /* LIBVLC_NEED_RWLOCK */

#ifdef LIBVLC_NEED_SEMAPHORE
/*** Generic semaphores ***/
#include <limits.h>
#include <errno.h>

void vlc_sem_init (vlc_sem_t *sem, unsigned value)
{
    vlc_mutex_init (&sem->lock);
    vlc_cond_init (&sem->wait);
    sem->value = value;
}

void vlc_sem_destroy (vlc_sem_t *sem)
{
    vlc_cond_destroy (&sem->wait);
    vlc_mutex_destroy (&sem->lock);
}

int vlc_sem_post (vlc_sem_t *sem)
{
    int ret = 0;

    vlc_mutex_lock (&sem->lock);
    if (likely(sem->value != UINT_MAX))
        sem->value++;
    else
        ret = EOVERFLOW;
    vlc_mutex_unlock (&sem->lock);
    vlc_cond_signal (&sem->wait);

    return ret;
}

void vlc_sem_wait (vlc_sem_t *sem)
{
    vlc_mutex_lock (&sem->lock);
    mutex_cleanup_push (&sem->lock);
    while (!sem->value)
        vlc_cond_wait (&sem->wait, &sem->lock);
    sem->value--;
    vlc_cleanup_pop ();
    vlc_mutex_unlock (&sem->lock);
}
#endif /* LIBVLC_NEED_SEMAPHORE */
