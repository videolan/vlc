/*****************************************************************************
 * picture_pool.c : picture pool functions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * Copyright (C) 2009 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 * Copyright (C) 2013-2015 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_picture_pool.h>
#include "picture.h"

#define POOL_MAX (CHAR_BIT * sizeof (unsigned long long))

static_assert ((POOL_MAX & (POOL_MAX - 1)) == 0, "Not a power of two");

struct picture_pool_t {
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    bool               canceled;
    unsigned long long available;
    atomic_ushort      refs;
    unsigned short     picture_count;
    picture_t  *picture[];
};

static void picture_pool_Destroy(picture_pool_t *pool)
{
    if (atomic_fetch_sub_explicit(&pool->refs, 1, memory_order_release) != 1)
        return;

    atomic_thread_fence(memory_order_acquire);
    aligned_free(pool);
}

void picture_pool_Release(picture_pool_t *pool)
{
    for (unsigned i = 0; i < pool->picture_count; i++)
        picture_Release(pool->picture[i]);
    picture_pool_Destroy(pool);
}

static void picture_pool_ReleaseClone(picture_t *clone)
{
    picture_priv_t *priv = (picture_priv_t *)clone;
    uintptr_t sys = (uintptr_t)priv->gc.opaque;
    picture_pool_t *pool = (void *)(sys & ~(POOL_MAX - 1));
    unsigned offset = sys & (POOL_MAX - 1);
    picture_t *picture = pool->picture[offset];

    picture_Release(picture);

    vlc_mutex_lock(&pool->lock);
    assert(!(pool->available & (1ULL << offset)));
    pool->available |= 1ULL << offset;
    vlc_cond_signal(&pool->wait);
    vlc_mutex_unlock(&pool->lock);

    picture_pool_Destroy(pool);
}

static picture_t *picture_pool_ClonePicture(picture_pool_t *pool,
                                            unsigned offset)
{
    picture_t *picture = pool->picture[offset];
    uintptr_t sys = ((uintptr_t)pool) + offset;

    return picture_InternalClone(picture, picture_pool_ReleaseClone,
                                 (void*)sys);
}

picture_pool_t *picture_pool_New(unsigned count, picture_t *const *tab)
{
    if (unlikely(count > POOL_MAX))
        return NULL;

    picture_pool_t *pool;
    size_t size = sizeof (*pool) + count * sizeof (picture_t *);

    size += (-size) & (POOL_MAX - 1);
    pool = aligned_alloc(POOL_MAX, size);
    if (unlikely(pool == NULL))
        return NULL;

    vlc_mutex_init(&pool->lock);
    vlc_cond_init(&pool->wait);
    if (count == POOL_MAX)
        pool->available = ~0ULL;
    else
        pool->available = (1ULL << count) - 1;
    atomic_init(&pool->refs,  1);
    pool->picture_count = count;
    memcpy(pool->picture, tab, count * sizeof (picture_t *));
    pool->canceled = false;
    return pool;
}

picture_pool_t *picture_pool_NewFromFormat(const video_format_t *fmt,
                                           unsigned count)
{
    picture_t *picture[count ? count : 1];
    unsigned i;

    for (i = 0; i < count; i++) {
        picture[i] = picture_NewFromFormat(fmt);
        if (picture[i] == NULL)
            goto error;
    }

    picture_pool_t *pool = picture_pool_New(count, picture);
    if (!pool)
        goto error;

    return pool;

error:
    while (i > 0)
        picture_Release(picture[--i]);
    return NULL;
}

picture_pool_t *picture_pool_Reserve(picture_pool_t *master, unsigned count)
{
    picture_t *picture[count ? count : 1];
    unsigned i;

    for (i = 0; i < count; i++) {
        picture[i] = picture_pool_Get(master);
        if (picture[i] == NULL)
            goto error;
    }

    picture_pool_t *pool = picture_pool_New(count, picture);
    if (!pool)
        goto error;

    return pool;

error:
    while (i > 0)
        picture_Release(picture[--i]);
    return NULL;
}

picture_t *picture_pool_Get(picture_pool_t *pool)
{
    unsigned long long available;

    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);
    available = pool->available;

    while (available != 0)
    {
        int i = ctz(available);

        if (unlikely(pool->canceled))
            break;

        pool->available &= ~(1ULL << i);
        vlc_mutex_unlock(&pool->lock);
        available &= ~(1ULL << i);

        picture_t *clone = picture_pool_ClonePicture(pool, i);
        if (clone != NULL) {
            assert(clone->p_next == NULL);
            atomic_fetch_add_explicit(&pool->refs, 1, memory_order_relaxed);
        }
        return clone;
    }

    vlc_mutex_unlock(&pool->lock);
    return NULL;
}

picture_t *picture_pool_Wait(picture_pool_t *pool)
{
    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);

    while (pool->available == 0)
    {
        if (pool->canceled)
        {
            vlc_mutex_unlock(&pool->lock);
            return NULL;
        }
        vlc_cond_wait(&pool->wait, &pool->lock);
    }

    int i = ctz(pool->available);
    pool->available &= ~(1ULL << i);
    vlc_mutex_unlock(&pool->lock);

    picture_t *clone = picture_pool_ClonePicture(pool, i);
    if (clone != NULL) {
        assert(clone->p_next == NULL);
        atomic_fetch_add_explicit(&pool->refs, 1, memory_order_relaxed);
    }
    return clone;
}

void picture_pool_Cancel(picture_pool_t *pool, bool canceled)
{
    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);

    pool->canceled = canceled;
    if (canceled)
        vlc_cond_broadcast(&pool->wait);
    vlc_mutex_unlock(&pool->lock);
}

unsigned picture_pool_GetSize(const picture_pool_t *pool)
{
    return pool->picture_count;
}
