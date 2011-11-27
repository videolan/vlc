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
struct picture_release_sys_t {
    /* Saved release */
    void (*release)(picture_t *);
    picture_release_sys_t *release_sys;

    /* */
    int  (*lock)(picture_t *);
    void (*unlock)(picture_t *);

    /* */
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
};

static void Release(picture_t *);
static int  Lock(picture_t *);
static void Unlock(picture_t *);

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
    return pool;
}

picture_pool_t *picture_pool_NewExtended(const picture_pool_configuration_t *cfg)
{
    picture_pool_t *pool = Create(NULL, cfg->picture_count);
    if (!pool)
        return NULL;

    for (int i = 0; i < cfg->picture_count; i++) {
        picture_t *picture = cfg->picture[i];

        /* The pool must be the only owner of the picture */
        assert(picture->i_refcount == 1);

        /* Install the new release callback */
        picture_release_sys_t *release_sys = malloc(sizeof(*release_sys));
        if (!release_sys)
            abort();
        release_sys->release     = picture->pf_release;
        release_sys->release_sys = picture->p_release_sys;
        release_sys->lock        = cfg->lock;
        release_sys->unlock      = cfg->unlock;
        release_sys->tick        = 0;

        /* */
        picture->i_refcount    = 0;
        picture->pf_release    = Release;
        picture->p_release_sys = release_sys;

        /* */
        pool->picture[i] = picture;
        pool->picture_reserved[i] = false;
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

    int found = 0;
    for (int i = 0; i < master->picture_count && found < count; i++) {
        if (master->picture_reserved[i])
            continue;

        assert(master->picture[i]->i_refcount == 0);
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
            picture_release_sys_t *release_sys = picture->p_release_sys;

            assert(picture->i_refcount == 0);
            assert(!pool->picture_reserved[i]);

            /* Restore old release callback */
            picture->i_refcount    = 1;
            picture->pf_release    = release_sys->release;
            picture->p_release_sys = release_sys->release_sys;

            picture_Release(picture);

            free(release_sys);
        }
    }
    free(pool->picture_reserved);
    free(pool->picture);
    free(pool);
}

picture_t *picture_pool_Get(picture_pool_t *pool)
{
    for (int i = 0; i < pool->picture_count; i++) {
        if (pool->picture_reserved[i])
            continue;

        picture_t *picture = pool->picture[i];
        if (picture->i_refcount > 0)
            continue;

        if (Lock(picture))
            continue;

        /* */
        picture->p_next = NULL;
        picture->p_release_sys->tick = pool->tick++;
        picture_Hold(picture);
        return picture;
    }
    return NULL;
}

void picture_pool_NonEmpty(picture_pool_t *pool, bool reset)
{
    picture_t *old = NULL;

    for (int i = 0; i < pool->picture_count; i++) {
        if (pool->picture_reserved[i])
            continue;

        picture_t *picture = pool->picture[i];
        if (reset) {
            if (picture->i_refcount > 0)
                Unlock(picture);
            picture->i_refcount = 0;
        } else if (picture->i_refcount == 0) {
            return;
        } else if (!old || picture->p_release_sys->tick < old->p_release_sys->tick) {
            old = picture;
        }
    }
    if (!reset && old) {
        if (old->i_refcount > 0)
            Unlock(old);
        old->i_refcount = 0;
    }
}
int picture_pool_GetSize(picture_pool_t *pool)
{
    return pool->picture_count;
}

static void Release(picture_t *picture)
{
    assert(picture->i_refcount > 0);

    if (--picture->i_refcount > 0)
        return;
    Unlock(picture);
}

static int Lock(picture_t *picture)
{
    picture_release_sys_t *release_sys = picture->p_release_sys;
    if (release_sys->lock)
        return release_sys->lock(picture);
    return VLC_SUCCESS;
}
static void Unlock(picture_t *picture)
{
    picture_release_sys_t *release_sys = picture->p_release_sys;
    if (release_sys->unlock)
        release_sys->unlock(picture);
}

