/*****************************************************************************
 * va_surface.c: libavcodec Generic Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * Copyright (C) 2015 Steve Lhomme
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Steve Lhomme <robux4@gmail.com>
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

#include <vlc_common.h>
#include <vlc_codecs.h>
#include <vlc_codec.h>
#include <vlc_picture.h>

#include "va_surface.h"

#include "avcodec.h"

struct vlc_va_surface_t {
    size_t               index;
    atomic_uintptr_t     refcount; // 1 ref for the surface existance, 1 per surface/clone in-flight
    va_pool_t            *va_pool;
};

struct va_pool_t
{
    /* */
    size_t       surface_count;
    unsigned     surface_width;
    unsigned     surface_height;

    vlc_va_surface_t surface[MAX_SURFACE_COUNT];

    struct va_pool_cfg callbacks;

    atomic_uintptr_t  poolrefs; // 1 ref for the pool creator, 1 ref per surface alive
    vlc_sem_t    available_surfaces;
};

static void va_pool_AddRef(va_pool_t *va_pool)
{
    atomic_fetch_add(&va_pool->poolrefs, 1);
}

static void va_pool_Release(va_pool_t *va_pool)
{
    if (atomic_fetch_sub(&va_pool->poolrefs, 1) != 1)
        return;

    va_pool->callbacks.pf_destroy_device(va_pool->callbacks.opaque);

    free(va_pool);
}

/* */
int va_pool_SetupDecoder(vlc_va_t *va, va_pool_t *va_pool, AVCodecContext *avctx,
                         const video_format_t *fmt, size_t count)
{
    if ( va_pool->surface_count >= count &&
         va_pool->surface_width  == fmt->i_width &&
         va_pool->surface_height == fmt->i_height )
    {
        msg_Dbg(va, "reusing surface pool");
        goto done;
    }

    /* */
    msg_Dbg(va, "va_pool_SetupDecoder id %d %dx%d count: %zu", avctx->codec_id, avctx->coded_width, avctx->coded_height, count);

    if (count > MAX_SURFACE_COUNT)
    {
        msg_Err(va, "too many surfaces requested %zu (max %d)", count, MAX_SURFACE_COUNT);
        return VLC_EGENERIC;
    }

    int err = va_pool->callbacks.pf_create_decoder_surfaces(va, avctx->codec_id, fmt, count);
    if (err != VLC_SUCCESS)
        return err;

    va_pool->surface_width  = fmt->i_width;
    va_pool->surface_height = fmt->i_height;
    va_pool->surface_count = count;

    vlc_sem_init(&va_pool->available_surfaces, count);

    for (size_t i = 0; i < va_pool->surface_count; i++) {
        vlc_va_surface_t *surface = &va_pool->surface[i];
        atomic_init(&surface->refcount, 1);
        va_pool_AddRef(va_pool);
        surface->index = i;
        surface->va_pool = va_pool;
    }
done:
    va_pool->callbacks.pf_setup_avcodec_ctx(va_pool->callbacks.opaque, avctx);

    return VLC_SUCCESS;
}

static vlc_va_surface_t *GetSurface(va_pool_t *va_pool)
{
    for (unsigned i = 0; i < va_pool->surface_count; i++) {
        vlc_va_surface_t *surface = &va_pool->surface[i];
        uintptr_t expected = 1;

        if (atomic_compare_exchange_strong(&surface->refcount, &expected, 2))
        {
            /* the copy should have added an extra reference */
            atomic_fetch_sub(&surface->refcount, 1);
            va_surface_AddRef(surface);
            return surface;
        }
    }
    return NULL;
}

vlc_va_surface_t *va_pool_Get(va_pool_t *va_pool)
{
    vlc_va_surface_t *surface;

    if (va_pool->surface_count == 0)
        return NULL;

    vlc_sem_wait(&va_pool->available_surfaces);
    surface = GetSurface(va_pool);
    assert(surface != NULL);
    return surface;
}

void va_surface_AddRef(vlc_va_surface_t *surface)
{
    atomic_fetch_add(&surface->refcount, 1);
}

void va_surface_Release(vlc_va_surface_t *surface)
{
    uintptr_t had_refcount = atomic_fetch_sub(&surface->refcount, 1);
    if (had_refcount == 2)
    {
        // the surface is not used anymore
        vlc_sem_post(&surface->va_pool->available_surfaces);
    }
    else if (had_refcount == 1)
    {
        // the surface has been released
        va_pool_Release(surface->va_pool);
    }
}

size_t va_surface_GetIndex(const vlc_va_surface_t *surface)
{
    return surface->index;
}

void va_pool_Close(va_pool_t *va_pool)
{
    for (unsigned i = 0; i < va_pool->surface_count; i++)
        va_surface_Release(&va_pool->surface[i]);
    va_pool->surface_count = 0;

    va_pool_Release(va_pool);
}

va_pool_t * va_pool_Create(vlc_va_t *va, const struct va_pool_cfg *cbs)
{
    va_pool_t *va_pool = malloc(sizeof(*va_pool));
    if (unlikely(va_pool == NULL))
        return NULL;

    va_pool->callbacks = *cbs;

    /* */
    if (cbs->pf_create_device(va)) {
        msg_Err(va, "Failed to create device");
        return NULL;
    }
    msg_Dbg(va, "CreateDevice succeed");

    va_pool->surface_count = 0;
    atomic_init(&va_pool->poolrefs, 1);

    return va_pool;
}
