/*****************************************************************************
 * hw_pool.c: hw based picture pool
 *****************************************************************************
 * Copyright (C) 2019-2020 VLC authors and VideoLAN
 *
 * Authors: Jai Luthra <me@jailuthra.in>
 *          Quentin Chateau <quentin.chateau@deepskycorp.com>
 *          Steve Lhomme <robux4@videolabs.io>
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

#include <vlc_picture_pool.h>
#include <vlc_atomic.h>
#include "hw_pool.h"

struct nvdec_pool_t {
    vlc_video_context           *vctx;

    nvdec_pool_owner_t          *owner;

    void                        *res[64];
    size_t                      pool_size;

    picture_pool_t              *picture_pool;

    vlc_atomic_rc_t             rc;
};

void nvdec_pool_AddRef(nvdec_pool_t *pool)
{
    vlc_atomic_rc_inc(&pool->rc);
}

void nvdec_pool_Release(nvdec_pool_t *pool)
{
    if (!vlc_atomic_rc_dec(&pool->rc))
        return;

    pool->owner->release_resources(pool->owner, pool->res, pool->pool_size);

    picture_pool_Release(pool->picture_pool);
    vlc_video_context_Release(pool->vctx);
}

nvdec_pool_t* nvdec_pool_Create(nvdec_pool_owner_t *owner,
                                const video_format_t *fmt, vlc_video_context *vctx,
                                void *buffers[], size_t pics_count)
{
    nvdec_pool_t *pool = calloc(1, sizeof(*pool));
    if (unlikely(!pool))
        return NULL;

    picture_t *pics[pics_count];
    for (size_t i=0; i < pics_count; i++)
    {
        pics[i] = picture_NewFromResource(fmt, &(picture_resource_t){ 0 });
        if (!pics[i])
        {
            while (i--)
                picture_Release(pics[i]);
            goto error;
        }
        pool->res[i] = buffers[i];
        pics[i]->p_sys = buffers[i];
    }

    pool->picture_pool = picture_pool_New(pics_count, pics);
    if (!pool->picture_pool)
        goto free_pool;

    pool->owner = owner;
    pool->vctx = vctx;
    pool->pool_size = pics_count;
    vlc_video_context_Hold(pool->vctx);

    vlc_atomic_rc_init(&pool->rc);
    return pool;

free_pool:
    for (size_t i=0; i < pics_count; i++)
    {
        if (pics[i] != NULL)
            picture_Release(pics[i]);
    }
error:
    free(pool);
    return NULL;
}

picture_t* nvdec_pool_Wait(nvdec_pool_t *pool)
{
    picture_t *pic = picture_pool_Wait(pool->picture_pool);
    if (!pic)
        return NULL;

    void *surface = pic->p_sys;
    pic->p_sys = NULL;
    pic->context = pool->owner->attach_picture(pool->owner, pool, surface);
    if (likely(pic->context != NULL))
        return pic;

    picture_Release(pic);
    return NULL;
}
