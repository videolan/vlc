/*****************************************************************************
 * picture_pool.c : picture pool functions
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * Copyright (C) 2009 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
    int64_t   tick;
    /* */
    int       picture_count;
    picture_t **picture;
};

static void PicturePoolPictureRelease(picture_t *);

picture_pool_t *picture_pool_NewExtended(const picture_pool_configuration_t *cfg)
{
    picture_pool_t *pool = calloc(1, sizeof(*pool));
    if (!pool)
        return NULL;

    pool->tick = 1;
    pool->picture_count = cfg->picture_count;
    pool->picture = calloc(pool->picture_count, sizeof(*pool->picture));
    if (!pool->picture) {
        free(pool);
        return NULL;
    }

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
        picture->pf_release    = PicturePoolPictureRelease;
        picture->p_release_sys = release_sys;

        /* */
        pool->picture[i] = picture;
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

void picture_pool_Delete(picture_pool_t *pool)
{
    for (int i = 0; i < pool->picture_count; i++) {
        picture_t *picture = pool->picture[i];
        picture_release_sys_t *release_sys = picture->p_release_sys;

        assert(picture->i_refcount == 0);

        /* Restore old release callback */
        picture->i_refcount    = 1;
        picture->pf_release    = release_sys->release;
        picture->p_release_sys = release_sys->release_sys;

        picture_Release(picture);

        free(release_sys);
    }
    free(pool->picture);
    free(pool);
}

picture_t *picture_pool_Get(picture_pool_t *pool)
{
    for (int i = 0; i < pool->picture_count; i++) {
        picture_t *picture = pool->picture[i];
        if (picture->i_refcount > 0)
            continue;

        picture_release_sys_t *release_sys = picture->p_release_sys;
        if (release_sys->lock && release_sys->lock(picture))
            continue;

        /* */
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
        picture_t *picture = pool->picture[i];

        if (reset) {
            /* TODO pf_unlock */
            picture->i_refcount = 0;
        } else if (picture->i_refcount == 0) {
            return;
        } else if (!old || picture->p_release_sys->tick < old->p_release_sys->tick) {
            old = picture;
        }
    }
    if (!reset && old) {
        /* TODO pf_unlock */
        old->i_refcount = 0;
    }
}

static void PicturePoolPictureRelease(picture_t *picture)
{
    assert(picture->i_refcount > 0);

    if (--picture->i_refcount > 0)
        return;

    picture_release_sys_t *release_sys = picture->p_release_sys;
    if (release_sys->unlock)
        release_sys->unlock(picture);
}

