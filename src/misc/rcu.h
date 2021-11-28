/**
 * \file rcu.h Read-Copy-Update (RCU) declarations
 * \ingroup rcu
 */
/*****************************************************************************
 * Copyright © 2021 Rémi Denis-Courmont
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

#ifndef VLC_RCU_H_
#define VLC_RCU_H_

#include <stdatomic.h>

/**
 * \defgroup rcu Read-Copy-Update synchronisation
 * \ingroup threads
 * The Read-Copy-Update (RCU) mechanism is a paradigm of memory synchronisation
 * first popularised by the Linux kernel.
 * It is essentially a mean to safely use atomic pointers, without encountering
 * use-after-free and other lifecycle bugs.
 *
 * It is an advantageous substitute for
 * the classic reader/writer lock with the following notable differences:
 * - The read side is completely lock-free and wait-free.
 * - The write side is guaranteed to make forward progress even if there are
 *   perpetually active readers, provided that all read sides are each
 *   individually finitely long.
 * - The protected data consists of a single atomic object,
 *   typically an atomic pointer
 *   (not to be confused with a pointer to an atomic object).
 * - There is no synchronisation object. RCU maintains its state globally,
 *   and with a modicum of per-thread state.
 *
 * The practical use of RCU differs from reader/writer lock in the following
 * ways:
 * - There is no synchronisation object, so there are no needs for
 *   initialisation and destruction.
 * - The directly protected objects are atom, so they must be accessed with
 *   atomic type-generics. Specifically:
 *   - Objects are read with atomic load-acquire.
 *   - Objects are written with atomic store-release.
 * - There are no writer lock and unlock functions. Instead there is a
 *   synchronisation function, vlc_rcu_synchronize(), which waits for all
 *   earlier read-side critical sections to complete.
 * - Synchronisation between concurrent writers is not provided. If multiple
 *   writers can race against each other, a separate mutual exclusion lock
 *   for interlocking.
 *
 * @{
 */

/**
 * Begins a read-side RCU critical section.
 *
 * This function marks the beginning of a read-side RCU critical section.
 * For the duration of the critical section, RCU-protected data is guaranteed
 * to remain valid, although it might be slightly stale.
 *
 * To access RCU-protect data, an atomic load-acquire should be used.
 *
 * \note Read-side RCU critical section can be nested.
 */
void vlc_rcu_read_lock(void);

/**
 * Ends a read-side RCU critical section.
 *
 * This function marks the end of a read-side RCU critical section. After this
 * function is called, RCU-protected data may be destroyed and can no longer
 * be accessed safely.
 *
 * \note In the case of nested critical sections, this function notionally ends
 * the inner-most critical section. In practice, this makes no differences, as
 * the calling thread remains in a critical section until the number of
 * vlc_rcu_read_unlock() calls equals that of vlc_rcu_read_lock() calls.
 */
void vlc_rcu_read_unlock(void);

/**
 * Checks if the thread is in an read-side RCU critical section.
 *
 * This function checks if the thread is in a middle of one or more read-side
 * RCU critical section(s). It has no side effects and is primarily meant for
 * self-debugging.
 *
 * \retval true the calling thread is in a read-side RCU critical section.
 * \retval false the calling thread is not in a read-side RCU critical section.
 */
VLC_USED
bool vlc_rcu_read_held(void);

/**
 * Waits for completion of earlier read-side RCU critical section.
 *
 * This functions waits until all read-side RCU critical sections that had
 * begun before to complete. Then it is safe to release resources associated
 * with the earlier value(s) of any RCU-protected atomic object.
 */
void vlc_rcu_synchronize(void);

/** @} */
#endif /* !VLC_RCU_H_ */
