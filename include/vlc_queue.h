/*****************************************************************************
 * vlc_queue.h: generic queue functions
 *****************************************************************************
 * Copyright (C) 2020 Rémi Denis-Courmont
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

#ifndef VLC_QUEUE_H
#define VLC_QUEUE_H

/**
 * @defgroup queue Thread-safe queues (FIFO)
 * @ingroup cext
 * @{
 * @file vlc_queue.h
 */

#include <stdbool.h>
#include <stdint.h>
#include <vlc_common.h>

/**
 * Opaque type for queue entry.
 */
struct vlc_queue_entry;

/**
 * Thread-safe queue (a.k.a. FIFO).
 */
typedef struct vlc_queue
{
    struct vlc_queue_entry *first;
    struct vlc_queue_entry **lastp;
    ptrdiff_t next_offset;
    vlc_mutex_t lock;
    vlc_cond_t wait;
} vlc_queue_t;

/**
 * Initializes a queue.
 *
 * @param queue storage space for the queue
 * @param next_offset offset of the pointer to the next element
 *                    within a queue entry (as per @c offsetof())
 */
VLC_API void vlc_queue_Init(vlc_queue_t *queue, ptrdiff_t next_offset);

/**
 * @defgroup queue_ll Queue internals
 *
 * Low-level queue functions.
 *
 * In some cases, the high-level queue functions do not exactly fit the
 * use case requirements, and it is necessary to access the queue internals.
 * This typically occurs when threads wait for elements to be added to the
 * queue at the same time as some other type of events.
 * @{
 */
/**
 * Locks a queue.
 *
 * No more than one thread can lock a queue at any given time, and no other
 * thread can modify the queue while it is locked.
 * Accordingly, if the queue is already locked by another thread, this function
 * waits.
 *
 * Use vlc_queue_Unlock() to release the lock.
 *
 * @warning Recursively locking a single queue is undefined.
 * Also locking more than one queue at a time may lead to lock inversion:
 * mind the locking order!
 */
static inline void vlc_queue_Lock(vlc_queue_t *q)
{
    vlc_mutex_lock(&q->lock);
}

/**
 * Unlocks a queue.
 *
 * This releases the lock on a queue, allowing other threads to manipulate the
 * queue. The behaviour is undefined if the calling thread is not holding the
 * queue lock.
 */
static inline void vlc_queue_Unlock(vlc_queue_t *q)
{
    vlc_mutex_unlock(&q->lock);
}

/**
 * Wakes one thread waiting for a queue entry up.
 */
static inline void vlc_queue_Signal(vlc_queue_t *q)
{
    vlc_cond_signal(&q->wait);
}

/**
 * Waits for a queue entry.
 *
 * @note This function is a cancellation point.
 * In case of cancellation, the queue will be locked,
 * as is consistent for condition variable semantics.
 *
 * @bug This function should probably not be aware of cancellation.
 */
static inline void vlc_queue_Wait(vlc_queue_t *q)
{
    vlc_cond_wait(&q->wait, &q->lock);
}

/**
 * Queues an entry (without locking).
 *
 * This function enqueues an entry, or rather a linked-list of entries, in a
 * thread-safe queue, without taking the queue lock.
 *
 * @warning It is assumed that the caller already holds the queue lock;
 * otherwise the behaviour is undefined.
 *
 * @param entry NULL-terminated list of entries to queue
 *              (if NULL, this function has no effects)
 */
VLC_API void vlc_queue_EnqueueUnlocked(vlc_queue_t *, void *entry);

/**
 * Dequeues the oldest entry (without locking).
 *
 * This function dequeues an entry from a thread-safe queue. It is assumed
 * that the caller already holds the queue lock; otherwise the behaviour is
 * undefined.
 *
 * @warning It is assumed that the caller already holds the queue lock;
 * otherwise the behaviour is undefined.
 *
 * @return the first entry in the queue, or NULL if the queue is empty
 */
VLC_API void *vlc_queue_DequeueUnlocked(vlc_queue_t *) VLC_USED;

/**
 * Dequeues all entries (without locking).
 *
 * This is equivalent to calling vlc_queue_DequeueUnlocked() repeatedly until
 * the queue is emptied. However this function is much faster than that, as it
 * does not need to update the linked-list pointers.
 *
 * @warning It is assumed that the caller already holds the queue lock;
 * otherwise the behaviour is undefined.
 *
 * @return a linked-list of all entries (possibly NULL if none)
 */
VLC_API void *vlc_queue_DequeueAllUnlocked(vlc_queue_t *) VLC_USED;

/**
 * Checks if a queue is empty (without locking).
 *
 * @warning It is assumed that the caller already holds the queue lock;
 * otherwise the behaviour is undefined.
 *
 * @retval false the queue contains one or more entries
 * @retval true the queue is empty
 */
VLC_USED static inline bool vlc_queue_IsEmpty(const vlc_queue_t *q)
{
    return q->first == NULL;
}

/** @} */

/**
 * Queues an entry.
 *
 * This function enqueues an entry, or rather a linked-list of entries, in a
 * thread-safe queue.
 *
 * @param entry list of entries (if NULL, this function has no effects)
 */
VLC_API void vlc_queue_Enqueue(vlc_queue_t *, void *entry);

/**
 * Dequeues the oldest entry.
 *
 * This function dequeues an entry from a thread-safe queue. If the queue is
 * empty, it will wait until at least one entry is available.
 *
 * @param offset offset of the next pointer within a queue entry
 *
 * @return the first entry in the queue, or NULL if the queue is empty
 */
VLC_API void *vlc_queue_Dequeue(vlc_queue_t *) VLC_USED;

/**
 * Dequeues all entries.
 *
 * This is equivalent to calling vlc_queue_Dequeue() repeatedly until the queue
 * is emptied. However this function is much faster than that, as it
 * does not need to update the linked-list pointers.
 *
 * @return a linked-list of all entries (possibly NULL if none)
 */
VLC_API void *vlc_queue_DequeueAll(vlc_queue_t *) VLC_USED;

/**
 * @defgroup queue_killable Killable queues
 *
 * Thread-safe queues with an end flag.
 *
 * @{
 */

/**
 * Marks a queue ended.
 */
static inline void vlc_queue_Kill(vlc_queue_t *q,
                                  bool *restrict tombstone)
{
    vlc_queue_Lock(q);
    *tombstone = true;
    vlc_queue_Signal(q);
    vlc_queue_Unlock(q);
}

/**
 * Dequeues one entry from a killable queue.
 *
 * @return an entry, or NULL if the queue is empty and has been ended.
 */
static inline void *vlc_queue_DequeueKillable(vlc_queue_t *q,
                                              bool *restrict tombstone)
{
    void *entry;

    vlc_queue_Lock(q);
    while (vlc_queue_IsEmpty(q) && !*tombstone)
        vlc_queue_Wait(q);

    entry = vlc_queue_DequeueUnlocked(q);
    vlc_queue_Unlock(q);
    return entry;
}

/** @} */

/** @} */
#endif
