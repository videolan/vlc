/*****************************************************************************
 * timer.c: simple threaded timer
 *****************************************************************************
 * Copyright (C) 2009-2012 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_atomic.h>

/*
 * POSIX timers are essentially unusable from a library: there provide no safe
 * way to ensure that a timer has no pending/ongoing iteration. Furthermore,
 * they typically require one thread per timer plus one thread per iteration,
 * which is inefficient and overkill (unless you need multiple iteration
 * of the same timer concurrently).
 * Thus, this is a generic manual implementation of timers using a thread.
 */

struct vlc_timer
{
    vlc_thread_t thread;
    vlc_cond_t   reschedule;
    vlc_mutex_t  lock;
    void       (*func) (void *);
    void        *data;
    mtime_t      value, interval;
    atomic_uint  overruns;
};

VLC_NORETURN
static void *vlc_timer_thread (void *data)
{
    struct vlc_timer *timer = data;

    vlc_mutex_lock (&timer->lock);
    mutex_cleanup_push (&timer->lock);

    for (;;)
    {
        while (timer->value == 0)
            vlc_cond_wait (&timer->reschedule, &timer->lock);

        if (vlc_cond_timedwait (&timer->reschedule, &timer->lock,
                                timer->value) == 0)
            continue;
        if (timer->interval == 0)
            timer->value = 0; /* disarm */
        vlc_mutex_unlock (&timer->lock);

        int canc = vlc_savecancel ();
        timer->func (timer->data);
        vlc_restorecancel (canc);

        mtime_t now = mdate ();
        unsigned misses;

        vlc_mutex_lock (&timer->lock);
        if (timer->interval == 0)
            continue;

        misses = (now - timer->value) / timer->interval;
        timer->value += timer->interval;
        /* Try to compensate for one miss (mwait() will return immediately)
         * but no more. Otherwise, we might busy loop, after extended periods
         * without scheduling (suspend, SIGSTOP, RT preemption, ...). */
        if (misses > 1)
        {
            misses--;
            timer->value += misses * timer->interval;
            atomic_fetch_add_explicit (&timer->overruns, misses,
                                       memory_order_relaxed);
        }
    }

    vlc_cleanup_pop ();
    assert (0);
}

/**
 * Initializes an asynchronous timer.
 * @warning Asynchronous timers are processed from an unspecified thread.
 * Multiple occurences of a single interval timer are serialized; they cannot
 * run concurrently.
 *
 * @param id pointer to timer to be initialized
 * @param func function that the timer will call
 * @param data parameter for the timer function
 * @return 0 on success, a system error code otherwise.
 */
int vlc_timer_create (vlc_timer_t *id, void (*func) (void *), void *data)
{
    struct vlc_timer *timer = malloc (sizeof (*timer));

    if (unlikely(timer == NULL))
        return ENOMEM;
    vlc_mutex_init (&timer->lock);
    vlc_cond_init (&timer->reschedule);
    assert (func);
    timer->func = func;
    timer->data = data;
    timer->value = 0;
    timer->interval = 0;
    atomic_init(&timer->overruns, 0);

    if (vlc_clone (&timer->thread, vlc_timer_thread, timer,
                   VLC_THREAD_PRIORITY_INPUT))
    {
        vlc_cond_destroy (&timer->reschedule);
        vlc_mutex_destroy (&timer->lock);
        free (timer);
        return ENOMEM;
    }

    *id = timer;
    return 0;
}

/**
 * Destroys an initialized timer. If needed, the timer is first disarmed.
 * This function is undefined if the specified timer is not initialized.
 *
 * @warning This function <b>must</b> be called before the timer data can be
 * freed and before the timer callback function can be unloaded.
 *
 * @param timer timer to destroy
 */
void vlc_timer_destroy (vlc_timer_t timer)
{
    vlc_cancel (timer->thread);
    vlc_join (timer->thread, NULL);
    vlc_cond_destroy (&timer->reschedule);
    vlc_mutex_destroy (&timer->lock);
    free (timer);
}

/**
 * Arm or disarm an initialized timer.
 * This functions overrides any previous call to itself.
 *
 * @note A timer can fire later than requested due to system scheduling
 * limitations. An interval timer can fail to trigger sometimes, either because
 * the system is busy or suspended, or because a previous iteration of the
 * timer is still running. See also vlc_timer_getoverrun().
 *
 * @param timer initialized timer
 * @param absolute the timer value origin is the same as mdate() if true,
 *                 the timer value is relative to now if false.
 * @param value zero to disarm the timer, otherwise the initial time to wait
 *              before firing the timer.
 * @param interval zero to fire the timer just once, otherwise the timer
 *                 repetition interval.
 */
void vlc_timer_schedule (vlc_timer_t timer, bool absolute,
                         mtime_t value, mtime_t interval)
{
    if (!absolute && value != 0)
        value += mdate();

    vlc_mutex_lock (&timer->lock);
    timer->value = value;
    timer->interval = interval;
    vlc_cond_signal (&timer->reschedule);
    vlc_mutex_unlock (&timer->lock);
}

/**
 * Fetch and reset the overrun counter for a timer.
 * @param timer initialized timer
 * @return the timer overrun counter, i.e. the number of times that the timer
 * should have run but did not since the last actual run. If all is well, this
 * is zero.
 */
unsigned vlc_timer_getoverrun (vlc_timer_t timer)
{
    return atomic_exchange_explicit (&timer->overruns, 0,
                                     memory_order_relaxed);
}
