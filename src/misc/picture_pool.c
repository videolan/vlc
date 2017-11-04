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
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_picture_pool.h>
#include <vlc_atomic.h>
#include "picture.h"

#define POOL_MAX (CHAR_BIT * sizeof (unsigned long long))

static_assert ((POOL_MAX & (POOL_MAX - 1)) == 0, "Not a power of two");

struct picture_pool_t {
    int       (*pic_lock)(picture_t *);
    void      (*pic_unlock)(picture_t *);
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
    if (atomic_fetch_sub(&pool->refs, 1) != 1)
        return;

    vlc_cond_destroy(&pool->wait);
    vlc_mutex_destroy(&pool->lock);
    aligned_free(pool);
}

void picture_pool_Release(picture_pool_t *pool)
{
    for (unsigned i = 0; i < pool->picture_count; i++)
        picture_Release(pool->picture[i]);
    picture_pool_Destroy(pool);
}

static void picture_pool_ReleasePicture(picture_t *clone)
{
    picture_priv_t *priv = (picture_priv_t *)clone;
    uintptr_t sys = (uintptr_t)priv->gc.opaque;
    picture_pool_t *pool = (void *)(sys & ~(POOL_MAX - 1));
    unsigned offset = sys & (POOL_MAX - 1);
    picture_t *picture = pool->picture[offset];

    free(clone);

    if (pool->pic_unlock != NULL)
        pool->pic_unlock(picture);
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
    picture_resource_t res = {
        .p_sys = picture->p_sys,
        .pf_destroy = picture_pool_ReleasePicture,
    };

    for (int i = 0; i < picture->i_planes; i++) {
        res.p[i].p_pixels = picture->p[i].p_pixels;
        res.p[i].i_lines = picture->p[i].i_lines;
        res.p[i].i_pitch = picture->p[i].i_pitch;
    }

    picture_t *clone = picture_NewFromResource(&picture->format, &res);
    if (likely(clone != NULL)) {
        ((picture_priv_t *)clone)->gc.opaque = (void *)sys;
        picture_Hold(picture);
    }
    return clone;
}

picture_pool_t *picture_pool_NewExtended(const picture_pool_configuration_t *cfg)
{
    if (unlikely(cfg->picture_count > POOL_MAX))
        return NULL;

    picture_pool_t *pool;
    size_t size = sizeof (*pool) + cfg->picture_count * sizeof (picture_t *);

    size += (-size) & (POOL_MAX - 1);
    pool = aligned_alloc(POOL_MAX, size);
    if (unlikely(pool == NULL))
        return NULL;

    pool->pic_lock   = cfg->lock;
    pool->pic_unlock = cfg->unlock;
    vlc_mutex_init(&pool->lock);
    vlc_cond_init(&pool->wait);
    if (cfg->picture_count == POOL_MAX)
        pool->available = ~0ULL;
    else
        pool->available = (1ULL << cfg->picture_count) - 1;
    atomic_init(&pool->refs,  1);
    pool->picture_count = cfg->picture_count;
    memcpy(pool->picture, cfg->picture,
           cfg->picture_count * sizeof (picture_t *));
    pool->canceled = false;
    return pool;
}

picture_pool_t *picture_pool_New(unsigned count, picture_t *const *tab)
{
    picture_pool_configuration_t cfg = {
        .picture_count = count,
        .picture = tab,
    };

    return picture_pool_NewExtended(&cfg);
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

/** Find next (bit) set */
static int fnsll(unsigned long long x, unsigned i)
{
    if (i >= CHAR_BIT * sizeof (x))
        return 0;
    return ffsll(x & ~((1ULL << i) - 1));
}

picture_t *picture_pool_Get(picture_pool_t *pool)
{
    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);

    if (pool->canceled)
    {
        vlc_mutex_unlock(&pool->lock);
        return NULL;
    }

    for (unsigned i = ffsll(pool->available); i; i = fnsll(pool->available, i))
    {
        pool->available &= ~(1ULL << (i - 1));
        vlc_mutex_unlock(&pool->lock);

        picture_t *picture = pool->picture[i - 1];

        if (pool->pic_lock != NULL && pool->pic_lock(picture) != VLC_SUCCESS) {
            vlc_mutex_lock(&pool->lock);
            pool->available |= 1ULL << (i - 1);
            continue;
        }

        picture_t *clone = picture_pool_ClonePicture(pool, i - 1);
        if (clone != NULL) {
            assert(clone->p_next == NULL);
            atomic_fetch_add(&pool->refs, 1);
        }
        return clone;
    }

    vlc_mutex_unlock(&pool->lock);
    return NULL;
}

picture_t *picture_pool_Wait(picture_pool_t *pool)
{
    unsigned i;

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

    i = ffsll(pool->available);
    assert(i > 0);
    pool->available &= ~(1ULL << (i - 1));
    vlc_mutex_unlock(&pool->lock);

    picture_t *picture = pool->picture[i - 1];

    if (pool->pic_lock != NULL && pool->pic_lock(picture) != VLC_SUCCESS) {
        vlc_mutex_lock(&pool->lock);
        pool->available |= 1ULL << (i - 1);
        vlc_cond_signal(&pool->wait);
        vlc_mutex_unlock(&pool->lock);
        return NULL;
    }

    picture_t *clone = picture_pool_ClonePicture(pool, i - 1);
    if (clone != NULL) {
        assert(clone->p_next == NULL);
        atomic_fetch_add(&pool->refs, 1);
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

bool picture_pool_OwnsPic(picture_pool_t *pool, picture_t *pic)
{
    picture_priv_t *priv = (picture_priv_t *)pic;

    while (priv->gc.destroy != picture_pool_ReleasePicture) {
        pic = priv->gc.opaque;
        priv = (picture_priv_t *)pic;
    }

    uintptr_t sys = (uintptr_t)priv->gc.opaque;
    picture_pool_t *picpool = (void *)(sys & ~(POOL_MAX - 1));
    return pool == picpool;
}

unsigned picture_pool_GetSize(const picture_pool_t *pool)
{
    return pool->picture_count;
}

void picture_pool_Enum(picture_pool_t *pool, void (*cb)(void *, picture_t *),
                       void *opaque)
{
    /* NOTE: So far, the pictures table cannot change after the pool is created
     * so there is no need to lock the pool mutex here. */
    for (unsigned i = 0; i < pool->picture_count; i++)
        cb(opaque, pool->picture[i]);
}
