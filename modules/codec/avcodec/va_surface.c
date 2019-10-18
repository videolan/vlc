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

#include "va_surface_internal.h"

#include "avcodec.h"

#define MAX_GET_RETRIES  ((VLC_TICK_FROM_SEC(1) + VOUT_OUTMEM_SLEEP) / VOUT_OUTMEM_SLEEP)

struct va_pool_t
{
    /* */
    unsigned     surface_count;
    unsigned     surface_width;
    unsigned     surface_height;

    vlc_va_surface_t *surface[MAX_SURFACE_COUNT];

    struct va_pool_cfg callbacks;
};

struct vlc_va_surface_t {
    unsigned             index;
    atomic_uintptr_t     refcount; // 1 ref for the surface existance, 1 per surface/clone in-flight
    picture_context_t    *pic_va_ctx;
    va_pool_t            *va_pool;
};

static void ReleasePoolSurfaces(va_pool_t *va_pool)
{
    for (unsigned i = 0; i < va_pool->surface_count; i++)
        va_surface_Release(va_pool->surface[i]);
    va_pool->callbacks.pf_destroy_surfaces(va_pool->callbacks.opaque);
    va_pool->surface_count = 0;
}

static int SetupSurfaces(vlc_va_t *, va_pool_t *);

/* */
int va_pool_SetupDecoder(vlc_va_t *va, va_pool_t *va_pool, const AVCodecContext *avctx,
                         const video_format_t *fmt, unsigned count)
{
    int err = VLC_ENOMEM;

    if ( va_pool->surface_count >= count &&
         va_pool->surface_width  == fmt->i_width &&
         va_pool->surface_height == fmt->i_height )
    {
        msg_Dbg(va, "reusing surface pool");
        err = VLC_SUCCESS;
        goto done;
    }

    /* */
    ReleasePoolSurfaces(va_pool);

    /* */
    msg_Dbg(va, "va_pool_SetupDecoder id %d %dx%d count: %d", avctx->codec_id, avctx->coded_width, avctx->coded_height, count);

    if (count > MAX_SURFACE_COUNT)
        return VLC_EGENERIC;

    err = va_pool->callbacks.pf_create_decoder_surfaces(va, avctx->codec_id, fmt, count);
    if (err == VLC_SUCCESS)
    {
        va_pool->surface_width  = fmt->i_width;
        va_pool->surface_height = fmt->i_height;
        va_pool->surface_count = count;
    }

done:
    if (err == VLC_SUCCESS)
        err = SetupSurfaces(va, va_pool);

    return err;
}

static int SetupSurfaces(vlc_va_t *va, va_pool_t *va_pool)
{
    int err = VLC_ENOMEM;

    for (unsigned i = 0; i < va_pool->surface_count; i++) {
        vlc_va_surface_t *p_surface = malloc(sizeof(*p_surface));
        if (unlikely(p_surface==NULL))
            goto done;
        p_surface->index = i;
        p_surface->va_pool = va_pool;
        p_surface->pic_va_ctx = va_pool->callbacks.pf_new_surface_context(va_pool->callbacks.opaque, p_surface);
        if (unlikely(p_surface->pic_va_ctx==NULL))
        {
            free(p_surface);
            goto done;
        }
        va_pool->surface[i] = p_surface;
        atomic_init(&p_surface->refcount, 1);
    }
    err = VLC_SUCCESS;

done:
    if (err == VLC_SUCCESS)
        va_pool->callbacks.pf_setup_avcodec_ctx(va_pool->callbacks.opaque);

    return err;
}

static vlc_va_surface_t *GetSurface(va_pool_t *va_pool)
{
    for (unsigned i = 0; i < va_pool->surface_count; i++) {
        vlc_va_surface_t *surface = va_pool->surface[i];
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
    unsigned tries = MAX_GET_RETRIES;
    vlc_va_surface_t *surface;

    if (va_pool->surface_count == 0)
        return NULL;

    while ((surface = GetSurface(va_pool)) == NULL)
    {
        if (--tries == 0)
            return NULL;
        /* Pool empty. Wait for some time as in src/input/decoder.c.
         * XXX: Both this and the core should use a semaphore or a CV. */
        vlc_tick_sleep(VOUT_OUTMEM_SLEEP);
    }
    return surface;
}

picture_context_t *va_surface_GetContext(vlc_va_surface_t *surface)
{
    return surface->pic_va_ctx;
}

void va_surface_AddRef(vlc_va_surface_t *surface)
{
    atomic_fetch_add(&surface->refcount, 1);
}

void va_surface_Release(vlc_va_surface_t *surface)
{
    if (atomic_fetch_sub(&surface->refcount, 1) != 1)
        return;
    free(surface);
}

unsigned va_surface_GetIndex(vlc_va_surface_t *surface)
{
    return surface->index;
}

void va_pool_Close(vlc_va_t *va, va_pool_t *va_pool)
{
    ReleasePoolSurfaces(va_pool);
    va_pool->callbacks.pf_destroy_device(va);
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

    return va_pool;
}

