/*****************************************************************************
 * queue.c: generic queue (FIFO)
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_queue.h>

/* Opaque struct type.
 *
 * ISO C uses the same representation for all pointer-to-struct types.
 * Still different pointer types are not compatible, i.e. cannot alias.
 * So use memcpy() to read/write pointer values.
 */
struct vlc_queue_entry;

static void entry_set(struct vlc_queue_entry **pp, struct vlc_queue_entry *e)
{
    memcpy(pp, &e, sizeof (e));
}

static struct vlc_queue_entry *entry_get(struct vlc_queue_entry *const *pp)
{
    struct vlc_queue_entry *e;

    memcpy(&e, pp, sizeof (e));
    return e;
}

static struct vlc_queue_entry **next_p(const struct vlc_queue_entry *e,
                                       ptrdiff_t offset)
{
    return (struct vlc_queue_entry **)(((unsigned char *)e) + offset);
}

static void next_set(struct vlc_queue_entry *e, struct vlc_queue_entry *next,
                     ptrdiff_t offset)
{
    return entry_set(next_p(e, offset), next);
}

static struct vlc_queue_entry *next_get(const struct vlc_queue_entry *e,
                                        ptrdiff_t offset)
{
    return entry_get(next_p(e, offset));
}

void vlc_queue_Init(vlc_queue_t *q, ptrdiff_t next_offset)
{
    q->first = NULL;
    q->lastp = &q->first;
    q->next_offset = next_offset;
    vlc_mutex_init(&q->lock);
    vlc_cond_init(&q->wait);
}

void vlc_queue_EnqueueUnlocked(vlc_queue_t *q, void *entry)
{
    struct vlc_queue_entry **lastp;
    const ptrdiff_t offset = q->next_offset;

    vlc_mutex_assert(&q->lock);
    assert(entry_get(q->lastp) == NULL);
    entry_set(q->lastp, entry);

    for (lastp = q->lastp; entry != NULL; entry = next_get(entry, offset))
        lastp = next_p(entry, offset);

    q->lastp = lastp;
    vlc_queue_Signal(q);
}

void *vlc_queue_DequeueUnlocked(vlc_queue_t *q)
{
    vlc_mutex_assert(&q->lock);

    void *entry = q->first;
    const ptrdiff_t offset = q->next_offset;

    if (entry != NULL) {
        struct vlc_queue_entry *next = next_get(entry, offset);

        next_set(entry, NULL, offset);
        q->first = next;

        if (next == NULL)
            q->lastp = &q->first;
    }

    return entry;
}

void *vlc_queue_DequeueAllUnlocked(vlc_queue_t *q)
{
    vlc_mutex_assert(&q->lock);

    void *entry = q->first;

    q->first = NULL;
    q->lastp = &q->first;

    return entry;
}

void vlc_queue_Enqueue(vlc_queue_t *q, void *entry)
{
    vlc_queue_Lock(q);
    vlc_queue_EnqueueUnlocked(q, entry);
    vlc_queue_Unlock(q);
}

void *vlc_queue_Dequeue(vlc_queue_t *q)
{
    void *entry;

    vlc_queue_Lock(q);
    vlc_testcancel();

    while (vlc_queue_IsEmpty(q))
        vlc_queue_Wait(q);

    entry = vlc_queue_DequeueUnlocked(q);
    vlc_queue_Unlock(q);

    return entry;
}

void *vlc_queue_DequeueAll(vlc_queue_t *q)
{
    void *entry;

    vlc_queue_Lock(q);
    entry = vlc_queue_DequeueAllUnlocked(q);
    vlc_queue_Unlock(q);

    return entry;
}
