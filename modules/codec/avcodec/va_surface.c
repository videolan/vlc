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
typedef int VA_PICSYS;
#include "va_surface.h"

#include "avcodec.h"

struct vlc_va_surface_t {
    atomic_uintptr_t     refcount;
    struct va_pic_context *pic_va_ctx;
};

static void DestroyVideoDecoder(vlc_va_sys_t *sys, va_pool_t *va_pool)
{
    for (unsigned i = 0; i < va_pool->surface_count; i++)
        va_surface_Release(va_pool->surface[i]);
    va_pool->callbacks->pf_destroy_surfaces(sys);
    va_pool->surface_count = 0;
}

static int SetupSurfaces(vlc_va_t *, va_pool_t *, unsigned count);

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
    DestroyVideoDecoder(va->sys, va_pool);

    /* */
    msg_Dbg(va, "va_pool_SetupDecoder id %d %dx%d count: %d", avctx->codec_id, avctx->coded_width, avctx->coded_height, count);

    if (count > MAX_SURFACE_COUNT)
        return VLC_EGENERIC;

    err = va_pool->callbacks->pf_create_decoder_surfaces(va, avctx->codec_id, fmt, count);
    if (err == VLC_SUCCESS)
    {
        va_pool->surface_width  = fmt->i_width;
        va_pool->surface_height = fmt->i_height;
        va_pool->surface_count = va_pool->can_extern_pool ? 0 : count;
    }

done:
    if (err == VLC_SUCCESS)
        err = SetupSurfaces(va, va_pool, count);

    return err;
}

static int SetupSurfaces(vlc_va_t *va, va_pool_t *va_pool, unsigned count)
{
    int err = VLC_ENOMEM;

    for (unsigned i = 0; i < va_pool->surface_count; i++) {
        vlc_va_surface_t *p_surface = malloc(sizeof(*p_surface));
        if (unlikely(p_surface==NULL))
            goto done;
        p_surface->pic_va_ctx = va_pool->callbacks->pf_new_surface_context(va, i, p_surface);
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
        va_pool->callbacks->pf_setup_avcodec_ctx(va->sys, count);

    return err;
}

static picture_context_t *GetSurface(va_pool_t *va_pool)
{
    for (unsigned i = 0; i < va_pool->surface_count; i++) {
        vlc_va_surface_t *surface = va_pool->surface[i];
        uintptr_t expected = 1;

        if (atomic_compare_exchange_strong(&surface->refcount, &expected, 2))
        {
            picture_context_t *field = surface->pic_va_ctx->s.copy(&surface->pic_va_ctx->s);
            /* the copy should have added an extra reference */
            atomic_fetch_sub(&surface->refcount, 1);
            return field;
        }
    }
    return NULL;
}

picture_context_t *va_pool_Get(va_pool_t *va_pool)
{
    unsigned tries = (VLC_TICK_FROM_SEC(1) + VOUT_OUTMEM_SLEEP) / VOUT_OUTMEM_SLEEP;
    picture_context_t *field;

    if (va_pool->surface_count == 0)
        return NULL;

    while ((field = GetSurface(va_pool)) == NULL)
    {
        if (--tries == 0)
            return NULL;
        /* Pool empty. Wait for some time as in src/input/decoder.c.
         * XXX: Both this and the core should use a semaphore or a CV. */
        vlc_tick_sleep(VOUT_OUTMEM_SLEEP);
    }
    return field;
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

void va_pool_Close(vlc_va_t *va, va_pool_t *va_pool)
{
    if (va_pool->callbacks)
    {
        DestroyVideoDecoder(va->sys, va_pool);
        va_pool->callbacks->pf_destroy_device(va);
    }
}

int va_pool_Open(vlc_va_t *va, const struct va_pool_cfg *cbs, va_pool_t *va_pool)
{
    va_pool->callbacks = cbs;

    /* */
    if (cbs->pf_create_device(va)) {
        msg_Err(va, "Failed to create device");
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "CreateDevice succeed");

    return VLC_SUCCESS;
}

