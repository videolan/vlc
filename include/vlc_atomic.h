/*****************************************************************************
 * vlc_atomic.h:
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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

#ifndef VLC_ATOMIC_H
# define VLC_ATOMIC_H

/**
 * \file
 * Atomic operations do not require locking, but they are not very powerful.
 */

# include <assert.h>
#ifndef __cplusplus
# include <stdatomic.h>
#else
# include <atomic>
using std::atomic_uintptr_t;
using std::memory_order_relaxed;
using std::memory_order_acq_rel;
#endif
# include <vlc_common.h>

#define VLC_STATIC_RC { \
    .refs = ATOMIC_VAR_INIT(0) \
}

typedef struct vlc_atomic_rc_t {
    atomic_uintptr_t refs;
} vlc_atomic_rc_t;

/** Init the RC to 1 */
static inline void vlc_atomic_rc_init(vlc_atomic_rc_t *rc)
{
    atomic_init(&rc->refs, (uintptr_t)1);
}

/** Increment the RC */
static inline void vlc_atomic_rc_inc(vlc_atomic_rc_t *rc)
{
    uintptr_t prev = atomic_fetch_add_explicit(&rc->refs, (uintptr_t)1,
                                               memory_order_relaxed);
    vlc_assert(prev);
    VLC_UNUSED(prev);
}

/** Decrement the RC and return true if it reaches 0 */
static inline bool vlc_atomic_rc_dec(vlc_atomic_rc_t *rc)
{
    uintptr_t prev = atomic_fetch_sub_explicit(&rc->refs, (uintptr_t)1,
                                               memory_order_acq_rel);
    vlc_assert(prev);
    return prev == 1;
}

/** Returns the current reference count.
 *  This is not safe to use for logic and must only be used for debugging or
 *  assertion purposes */
static inline uintptr_t vlc_atomic_rc_get(const vlc_atomic_rc_t* rc)
{
    return atomic_load_explicit(&rc->refs, memory_order_relaxed);
}

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
VLC_API void vlc_atomic_wait(void *addr, unsigned val);

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
VLC_API
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
VLC_API void vlc_atomic_notify_one(void *addr);

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
VLC_API void vlc_atomic_notify_all(void *addr);

#endif
