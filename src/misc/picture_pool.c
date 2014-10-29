/*****************************************************************************
 * picture_pool.c : picture pool functions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * Copyright (C) 2009 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 * $Id$
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture_pool.h>

/*****************************************************************************
 *
 *****************************************************************************/
struct picture_gc_sys_t {
    picture_pool_t *pool;
    picture_t *picture;
    bool in_use;
    uint64_t tick;
};

struct picture_pool_t {
    uint64_t       tick;
    /* */
    unsigned       picture_count;
    picture_t      **picture;

    int       (*pic_lock)(picture_t *);
    void      (*pic_unlock)(picture_t *);
    unsigned    refs;
    vlc_mutex_t lock;
};

void picture_pool_Release(picture_pool_t *pool)
{
    bool destroy;

    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);
    destroy = --pool->refs == 0;
    vlc_mutex_unlock(&pool->lock);

    if (likely(!destroy))
        return;

    for (unsigned i = 0; i < pool->picture_count; i++) {
        picture_t *picture = pool->picture[i];
        picture_gc_sys_t *sys = picture->gc.p_sys;

        picture_Release(sys->picture);
        free(sys);
        free(picture);
    }

    vlc_mutex_destroy(&pool->lock);
    free(pool->picture);
    free(pool);
}

static void picture_pool_ReleasePicture(picture_t *picture)
{
    picture_gc_sys_t *sys = picture->gc.p_sys;
    picture_pool_t *pool = sys->pool;

    if (pool->pic_unlock != NULL)
        pool->pic_unlock(picture);

    vlc_mutex_lock(&pool->lock);
    assert(sys->in_use);
    sys->in_use = false;
    vlc_mutex_unlock(&pool->lock);

    picture_pool_Release(pool);
}

static picture_t *picture_pool_ClonePicture(picture_pool_t *pool,
                                            picture_t *picture)
{
    picture_gc_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return NULL;

    sys->pool = pool;
    sys->picture = picture;
    sys->in_use = false;
    sys->tick = 0;

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
    if (likely(clone != NULL))
        clone->gc.p_sys = sys;
    else
        free(sys);

    return clone;
}

static picture_pool_t *Create(int picture_count)
{
    picture_pool_t *pool = calloc(1, sizeof(*pool));
    if (!pool)
        return NULL;

    pool->tick = 1;
    pool->picture_count = picture_count;
    pool->picture = calloc(pool->picture_count, sizeof(*pool->picture));
    if (!pool->picture) {
        free(pool->picture);
        free(pool);
        return NULL;
    }
    pool->refs = 1;
    vlc_mutex_init(&pool->lock);
    return pool;
}

picture_pool_t *picture_pool_NewExtended(const picture_pool_configuration_t *cfg)
{
    picture_pool_t *pool = Create(cfg->picture_count);
    if (!pool)
        return NULL;

    pool->pic_lock   = cfg->lock;
    pool->pic_unlock = cfg->unlock;

    for (unsigned i = 0; i < cfg->picture_count; i++) {
        picture_t *picture = picture_pool_ClonePicture(pool, cfg->picture[i]);
        if (unlikely(picture == NULL))
            abort();

        atomic_init(&picture->gc.refcount, 0);

        pool->picture[i] = picture;
    }
    return pool;

}

picture_pool_t *picture_pool_New(unsigned count, picture_t *const *tab)
{
    picture_pool_configuration_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.picture_count = count;
    cfg.picture       = tab;

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

picture_t *picture_pool_Get(picture_pool_t *pool)
{
    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);

    for (unsigned i = 0; i < pool->picture_count; i++) {
        picture_t *picture = pool->picture[i];
        picture_gc_sys_t *sys = picture->gc.p_sys;
        uint64_t tick;

        if (sys->in_use)
            continue;

        pool->refs++;
        tick = ++pool->tick;
        sys->in_use = true;
        vlc_mutex_unlock(&pool->lock);

        if (pool->pic_lock != NULL && pool->pic_lock(picture) != 0) {
            vlc_mutex_lock(&pool->lock);
            sys->in_use = false;
            pool->refs--;
            continue;
        }

        sys->tick = tick;

        assert(atomic_load(&picture->gc.refcount) == 0);
        atomic_init(&picture->gc.refcount, 1);
        picture->p_next = NULL;
        return picture;
    }

    vlc_mutex_unlock(&pool->lock);
    return NULL;
}

unsigned picture_pool_Reset(picture_pool_t *pool)
{
    unsigned ret = 0;
retry:
    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);

    for (unsigned i = 0; i < pool->picture_count; i++) {
        picture_t *picture = pool->picture[i];
        picture_gc_sys_t *sys = picture->gc.p_sys;

        if (sys->in_use) {
            vlc_mutex_unlock(&pool->lock);
            picture_Release(picture);
            ret++;
            goto retry;
        }
    }
    vlc_mutex_unlock(&pool->lock);

    return ret;
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
