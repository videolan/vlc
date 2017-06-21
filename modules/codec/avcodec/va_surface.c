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


#define D3D_DecoderSurface  void
typedef struct vlc_va_surface_t vlc_va_surface_t;
#include "va_surface_internal.h"

#include "avcodec.h"

static void DestroyVideoDecoder(vlc_va_t *va, va_pool_t *va_pool)
{
    va_pool->pf_destroy_surfaces(va);
    va_pool->surface_count = 0;
}

/* */
int va_pool_Setup(vlc_va_t *va, va_pool_t *va_pool, AVCodecContext *avctx, unsigned count, int alignment)
{
    int err = VLC_ENOMEM;
    unsigned i = va_pool->surface_count;

    if (avctx->coded_width <= 0 || avctx->coded_height <= 0)
        return VLC_EGENERIC;

    assert((alignment & (alignment - 1)) == 0); /* power of 2 */
#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))
    int surface_width  = ALIGN(avctx->coded_width,  alignment);
    int surface_height = ALIGN(avctx->coded_height, alignment);

    if (avctx->coded_width != surface_width || avctx->coded_height != surface_height)
        msg_Warn( va, "surface dimensions (%dx%d) differ from avcodec dimensions (%dx%d)",
                  surface_width, surface_height,
                  avctx->coded_width, avctx->coded_height);

    if (va_pool->surface_width == surface_width && va_pool->surface_height == surface_height)
    {
        err = VLC_SUCCESS;
        goto done;
    }

    /* */
    DestroyVideoDecoder(va, va_pool);

    /* */
    msg_Dbg(va, "va_pool_Setup id %d %dx%d count: %d", avctx->codec_id, avctx->coded_width, avctx->coded_height, count);

    if (count > MAX_SURFACE_COUNT)
        return VLC_EGENERIC;

    /* FIXME transmit a video_format_t by VaSetup directly */
    video_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.i_width  = surface_width;
    fmt.i_height = surface_height;
    fmt.i_frame_rate      = avctx->framerate.num;
    fmt.i_frame_rate_base = avctx->framerate.den;

    if (va_pool->pf_create_decoder_surfaces(va, avctx->codec_id, &fmt, count))
        return VLC_EGENERIC;

    for (i = 0; i < count; i++) {
        vlc_va_surface_t *surface = malloc(sizeof(*surface));
        if (unlikely(surface==NULL))
            goto done;
        atomic_init(&surface->refcount, 1);
        va_pool->surface[i] = surface;
    }

    va_pool->surface_width  = surface_width;
    va_pool->surface_height = surface_height;

    va_pool->pf_setup_avcodec_ctx(va);
    err = VLC_SUCCESS;

done:
    va_pool->surface_count = i;
    return err;
}

static vlc_va_surface_t *GetSurface(va_pool_t *va_pool)
{
    for (unsigned i = 0; i < va_pool->surface_count; i++) {
        vlc_va_surface_t *surface = va_pool->surface[i];
        uintptr_t expected = 1;

        if (atomic_compare_exchange_strong(&surface->refcount, &expected, 2))
        {
            /* TODO do a copy to allow releasing locally and keep forward alive atomic_fetch_sub(&surface->refs, 1);*/
            surface->decoderSurface = va_pool->hw_surface[i];
            return surface;
        }
    }
    return NULL;
}

vlc_va_surface_t *va_pool_Get(vlc_va_t *va, va_pool_t *va_pool)
{
    unsigned tries = (CLOCK_FREQ + VOUT_OUTMEM_SLEEP) / VOUT_OUTMEM_SLEEP;
    vlc_va_surface_t *field;

    while ((field = GetSurface(va_pool)) == NULL)
    {
        if (--tries == 0)
            return NULL;
        /* Pool empty. Wait for some time as in src/input/decoder.c.
         * XXX: Both this and the core should use a semaphore or a CV. */
        msleep(VOUT_OUTMEM_SLEEP);
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
    DestroyVideoDecoder(va, va_pool);
    va_pool->pf_destroy_video_service(va);
    if (va_pool->pf_destroy_device_manager)
        va_pool->pf_destroy_device_manager(va);
    va_pool->pf_destroy_device(va);
}

int va_pool_Open(vlc_va_t *va, va_pool_t *va_pool, AVCodecContext *ctx)
{
    va_pool->codec_id = ctx->codec_id;

    /* */
    if (va_pool->pf_create_device(va)) {
        msg_Err(va, "Failed to create device");
        goto error;
    }
    msg_Dbg(va, "CreateDevice succeed");

    if (va_pool->pf_create_device_manager &&
        va_pool->pf_create_device_manager(va) != VLC_SUCCESS) {
        msg_Err(va, "CreateDeviceManager failed");
        goto error;
    }

    if (va_pool->pf_create_video_service(va)) {
        msg_Err(va, "CreateVideoService failed");
        goto error;
    }

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

