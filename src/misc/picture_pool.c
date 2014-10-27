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
    /* Saved release */
    void (*destroy)(picture_t *);
    void *destroy_sys;
    /* */
    atomic_bool zombie;
    int64_t tick;
};

struct picture_pool_t {
    /* */
    picture_pool_t *master;
    int64_t        tick;
    /* */
    int            picture_count;
    picture_t      **picture;
    bool           *picture_reserved;

    int       (*pic_lock)(picture_t *);
    void      (*pic_unlock)(picture_t *);
    unsigned    refs;
    vlc_mutex_t lock;
};

static void Release(picture_pool_t *pool)
{
    bool destroy;

    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);
    destroy = !--pool->refs;
    vlc_mutex_unlock(&pool->lock);

    if (!destroy)
        return;

    vlc_mutex_destroy(&pool->lock);
    free(pool->picture_reserved);
    free(pool->picture);
    free(pool);
}

static void DestroyPicture(picture_t *picture)
{
    picture_gc_sys_t *gc_sys = picture->gc.p_sys;
    picture_pool_t *pool = gc_sys->pool;

    if (pool->pic_unlock != NULL)
        pool->pic_unlock(picture);

    if (!atomic_load(&gc_sys->zombie))
        return;

    /* Picture from an already destroyed pool */
    picture->gc.pf_destroy = gc_sys->destroy;
    picture->gc.p_sys      = gc_sys->destroy_sys;
    free(gc_sys);

    picture->gc.pf_destroy(picture);
    Release(pool);
}

static picture_pool_t *Create(picture_pool_t *master, int picture_count)
{
    picture_pool_t *pool = calloc(1, sizeof(*pool));
    if (!pool)
        return NULL;

    pool->master = master;
    pool->tick = master ? master->tick : 1;
    pool->picture_count = picture_count;
    pool->picture = calloc(pool->picture_count, sizeof(*pool->picture));
    pool->picture_reserved = calloc(pool->picture_count, sizeof(*pool->picture_reserved));
    if (!pool->picture || !pool->picture_reserved) {
        free(pool->picture);
        free(pool->picture_reserved);
        free(pool);
        return NULL;
    }
    pool->refs = 1;
    vlc_mutex_init(&pool->lock);
    return pool;
}

picture_pool_t *picture_pool_NewExtended(const picture_pool_configuration_t *cfg)
{
    picture_pool_t *pool = Create(NULL, cfg->picture_count);
    if (!pool)
        return NULL;

    pool->pic_lock   = cfg->lock;
    pool->pic_unlock = cfg->unlock;

    /*
     * NOTE: When a pooled picture is released, it must be returned to the list
     * of available pictures from its pool, rather than destroyed.
     * This requires a dedicated release callback, a pointer to the pool and a
     * reference count. For simplicity, rather than allocate a whole new
     * picture_t structure, the pool overrides gc.pf_destroy and gc.p_sys when
     * created, and restores them when destroyed.
     * There are some implications to keep in mind:
     *  - The original creator of the picture (e.g. video output display) must
     *    not manipulate the gc parameters while the picture is pooled.
     *  - The picture cannot be pooled more than once, in other words, pools
     *    cannot be stacked/layered.
     *  - The picture must be available and its reference count equal to one
     *    when it gets pooled.
     *  - Picture plane pointers and sizes must not be mangled in any case.
     */
    for (int i = 0; i < cfg->picture_count; i++) {
        picture_t *picture = cfg->picture[i];

        /* Save the original garbage collector */
        picture_gc_sys_t *gc_sys = malloc(sizeof(*gc_sys));
        if (unlikely(gc_sys == NULL))
            abort();
        gc_sys->pool        = pool;
        gc_sys->destroy     = picture->gc.pf_destroy;
        gc_sys->destroy_sys = picture->gc.p_sys;
        atomic_init(&gc_sys->zombie, false);
        gc_sys->tick        = 0;

        /* Override the garbage collector */
        assert(atomic_load(&picture->gc.refcount) == 1);
        atomic_init(&picture->gc.refcount, 0);
        picture->gc.pf_destroy = DestroyPicture;
        picture->gc.p_sys      = gc_sys;

        /* */
        pool->picture[i] = picture;
        pool->picture_reserved[i] = false;
        pool->refs++;
    }
    return pool;

}

picture_pool_t *picture_pool_New(int picture_count, picture_t *picture[])
{
    picture_pool_configuration_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.picture_count = picture_count;
    cfg.picture       = picture;

    return picture_pool_NewExtended(&cfg);
}

picture_pool_t *picture_pool_NewFromFormat(const video_format_t *fmt, int picture_count)
{
    picture_t *picture[picture_count];

    for (int i = 0; i < picture_count; i++) {
        picture[i] = picture_NewFromFormat(fmt);
        if (!picture[i])
            goto error;
    }
    picture_pool_t *pool = picture_pool_New(picture_count, picture);
    if (!pool)
        goto error;

    return pool;

error:
    for (int i = 0; i < picture_count; i++) {
        if (!picture[i])
            break;
        picture_Release(picture[i]);
    }
    return NULL;
}

picture_pool_t *picture_pool_Reserve(picture_pool_t *master, int count)
{
    picture_pool_t *pool = Create(master, count);
    if (!pool)
        return NULL;

    pool->pic_lock   = master->pic_lock;
    pool->pic_unlock = master->pic_unlock;

    int found = 0;
    for (int i = 0; i < master->picture_count && found < count; i++) {
        if (master->picture_reserved[i])
            continue;

        assert(atomic_load(&master->picture[i]->gc.refcount) == 0);
        master->picture_reserved[i] = true;

        pool->picture[found]          = master->picture[i];
        pool->picture_reserved[found] = false;
        found++;
    }
    if (found < count) {
        picture_pool_Delete(pool);
        return NULL;
    }
    return pool;
}

void picture_pool_Delete(picture_pool_t *pool)
{
    for (int i = 0; i < pool->picture_count; i++) {
        picture_t *picture = pool->picture[i];
        if (pool->master) {
            for (int j = 0; j < pool->master->picture_count; j++) {
                if (pool->master->picture[j] == picture)
                    pool->master->picture_reserved[j] = false;
            }
        } else {
            picture_gc_sys_t *gc_sys = picture->gc.p_sys;

            assert(!pool->picture_reserved[i]);

            /* Restore the initial reference that was cloberred in
             * picture_pool_NewExtended(). */
            atomic_fetch_add(&picture->gc.refcount, 1);
            /* The picture might still locked and then the G.C. state cannot be
             * modified (w/o memory synchronization). */
            atomic_store(&gc_sys->zombie, true);

            picture_Release(picture);
        }
    }
    Release(pool);
}

picture_t *picture_pool_Get(picture_pool_t *pool)
{
    for (int i = 0; i < pool->picture_count; i++) {
        if (pool->picture_reserved[i])
            continue;

        picture_t *picture = pool->picture[i];
        if (atomic_load(&picture->gc.refcount) > 0)
            continue;

        if (pool->pic_lock != NULL && pool->pic_lock(picture) != 0)
            continue;

        /* */
        picture->p_next = NULL;
        picture->gc.p_sys->tick = pool->tick++;
        picture_Hold(picture);
        return picture;
    }
    return NULL;
}

void picture_pool_Reset(picture_pool_t *pool)
{
    for (int i = 0; i < pool->picture_count; i++) {
        if (pool->picture_reserved[i])
            continue;

        picture_t *picture = pool->picture[i];
        if (atomic_load(&picture->gc.refcount) > 0) {
            if (pool->pic_unlock != NULL)
                pool->pic_unlock(picture);
        }
        atomic_store(&picture->gc.refcount, 0);
    }
}

void picture_pool_NonEmpty(picture_pool_t *pool)
{
    picture_t *oldest = NULL;

    for (int i = 0; i < pool->picture_count; i++) {
        if (pool->picture_reserved[i])
            continue;

        picture_t *picture = pool->picture[i];
        if (atomic_load(&picture->gc.refcount) == 0)
            return; /* Nothing to do */

        if (oldest == NULL || picture->gc.p_sys->tick < oldest->gc.p_sys->tick)
            oldest = picture;
    }

    if (oldest == NULL)
        return; /* Cannot fix! */

    if (atomic_load(&oldest->gc.refcount) > 0) {
        if (pool->pic_unlock != NULL)
            pool->pic_unlock(oldest);
    }
    atomic_store(&oldest->gc.refcount, 0);
}

int picture_pool_GetSize(picture_pool_t *pool)
{
    return pool->picture_count;
}
