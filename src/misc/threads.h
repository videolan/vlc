/*****************************************************************************
 * threads.h : core internal threads implementation for the VideoLAN client
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

#ifndef VLC_CORE_THREADS_H_
#define VLC_CORE_THREADS_H_

#include <vlc_threads.h>

/*
 * Queued mutex
 *
 * A queued mutex is a type of thread-safe mutual exclusion lock that is
 * acquired in strict FIFO order.
 *
 * In most cases, a regular mutex (\ref vlc_mutex_t) should be used instead.
 * There are important differences:
 * - A queued mutex is generally slower, especially on the fast path.
 * - A queued mutex cannot be combined with a condition variable.
 *   Indeed, the scheduling policy of the condition variable would typically
 *   conflict with that of the queued mutex, leading to a dead lock.
 * - The try-lock operation is not implemented.
 */
typedef struct {
    atomic_uint head;
    atomic_uint tail;
    atomic_ulong owner;
} vlc_queuedmutex_t;

#define VLC_STATIC_QUEUEDMUTEX { 0, 0, 0 }

void vlc_queuedmutex_init(vlc_queuedmutex_t *m);

void vlc_queuedmutex_lock(vlc_queuedmutex_t *m);

void vlc_queuedmutex_unlock(vlc_queuedmutex_t *m);

/**
 * Checks if a queued mutex is locked.
 *
 * This function checks if the calling thread holds a given queued mutual
 * exclusion lock. It has no side effects and is essentially intended for
 * run-time debugging.
 *
 * @note To assert that the calling thread holds a lock, the helper macro
 * vlc_queuedmutex_assert() should be used instead of this function.
 *
 * @retval false the mutex is not locked by the calling thread
 * @retval true the mutex is locked by the calling thread
 */
bool vlc_queuedmutex_held(vlc_queuedmutex_t *m);

#define vlc_queuedmutex_assert(m) assert(vlc_queuedmutex_held(m))

#endif /* !VLC_CORE_THREADS_H_ */
