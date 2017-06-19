/*****************************************************************************
 * avcodec.c: VDPAU decoder for libav
 *****************************************************************************
 * Copyright (C) 2012-2013 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/vdpau.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fourcc.h>
#include <vlc_picture.h>
#include <vlc_atomic.h>
#include <vlc_xlib.h>
#include "vlc_vdpau.h"
#include "../../codec/avcodec/va.h"

struct vlc_va_sys_t
{
    vdp_t *vdp;
    VdpDevice device;
    VdpChromaType type;
    uint32_t width;
    uint32_t height;
    vlc_vdp_video_field_t *pool[];
};

static vlc_vdp_video_field_t *CreateSurface(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    VdpVideoSurface surface;
    VdpStatus err;

    err = vdp_video_surface_create(sys->vdp, sys->device, sys->type,
                                   sys->width, sys->height, &surface);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(va, "%s creation failure: %s", "video surface",
                vdp_get_error_string(sys->vdp, err));
        return NULL;
    }

    vlc_vdp_video_field_t *field = vlc_vdp_video_create(sys->vdp, surface);
    if (unlikely(field == NULL))
        vdp_video_surface_destroy(sys->vdp, surface);
    return field;
}

static vlc_vdp_video_field_t *GetSurface(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    vlc_vdp_video_field_t *f;

    for (unsigned i = 0; (f = sys->pool[i]) != NULL; i++)
    {
        uintptr_t expected = 1;

        if (atomic_compare_exchange_strong(&f->frame->refs, &expected, 2))
        {
            vlc_vdp_video_field_t *field = vlc_vdp_video_copy(f);
            atomic_fetch_sub(&f->frame->refs, 1);
            return field;
        }
    }
    return NULL;
}

static int Lock(vlc_va_t *va, picture_t *pic, uint8_t **data)
{
    vlc_vdp_video_field_t *field;
    unsigned tries = (CLOCK_FREQ + VOUT_OUTMEM_SLEEP) / VOUT_OUTMEM_SLEEP;

    while ((field = GetSurface(va)) == NULL)
    {
        if (--tries == 0)
            return VLC_ENOMEM;
        /* Pool empty. Wait for some time as in src/input/decoder.c.
         * XXX: Both this and the core should use a semaphore or a CV. */
        msleep(VOUT_OUTMEM_SLEEP);
    }

    pic->context = &field->context;
    *data = (void *)(uintptr_t)field->frame->surface;
    return VLC_SUCCESS;
}

static int Open(vlc_va_t *va, AVCodecContext *avctx, enum PixelFormat pix_fmt,
                const es_format_t *fmt, picture_sys_t *p_sys)
{
    if (pix_fmt != AV_PIX_FMT_VDPAU)
        return VLC_EGENERIC;

    (void) fmt;
    (void) p_sys;
    void *func;
    VdpStatus err;
    VdpChromaType type;
    uint32_t width, height;

    if (av_vdpau_get_surface_parameters(avctx, &type, &width, &height))
        return VLC_EGENERIC;

    switch (type)
    {
        case VDP_CHROMA_TYPE_420:
        case VDP_CHROMA_TYPE_422:
        case VDP_CHROMA_TYPE_444:
            break;
        default:
            msg_Err(va, "unsupported chroma type %"PRIu32, type);
            return VLC_EGENERIC;
    }

    if (!vlc_xlib_init(VLC_OBJECT(va)))
    {
        msg_Err(va, "Xlib is required for VDPAU");
        return VLC_EGENERIC;
    }

    unsigned refs = avctx->refs + 2 * avctx->thread_count + 5;
    vlc_va_sys_t *sys = malloc(sizeof (*sys)
                               + (refs + 1) * sizeof (sys->pool[0]));
    if (unlikely(sys == NULL))
       return VLC_ENOMEM;

    sys->type = type;
    sys->width = width;
    sys->height = height;

    err = vdp_get_x11(NULL, -1, &sys->vdp, &sys->device);
    if (err != VDP_STATUS_OK)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    unsigned flags = AV_HWACCEL_FLAG_ALLOW_HIGH_DEPTH;

    err = vdp_get_proc_address(sys->vdp, sys->device,
                               VDP_FUNC_ID_GET_PROC_ADDRESS, &func);
    if (err != VDP_STATUS_OK)
        goto error;

    if (av_vdpau_bind_context(avctx, sys->device, func, flags))
        goto error;
    va->sys = sys;

    unsigned i = 0;
    while (i < refs)
    {
        sys->pool[i] = CreateSurface(va);
        if (sys->pool[i] == NULL)
            break;
        i++;
    }
    sys->pool[i] = NULL;

    if (i < avctx->refs + 3u)
    {
        msg_Err(va, "not enough video RAM");
        while (i > 0)
            vlc_vdp_video_destroy(sys->pool[--i]);
        goto error;
    }

    if (i < refs)
        msg_Warn(va, "video RAM low (allocated %u of %u buffers)",
                 i, refs);

    const char *infos;
    if (vdp_get_information_string(sys->vdp, &infos) != VDP_STATUS_OK)
        infos = "VDPAU";

    va->description = infos;
    va->get = Lock;
    return VLC_SUCCESS;

error:
    vdp_release_x11(sys->vdp);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_va_t *va, void **hwctx)
{
    vlc_va_sys_t *sys = va->sys;

    for (unsigned i = 0; sys->pool[i] != NULL; i++)
        vlc_vdp_video_destroy(sys->pool[i]);
    vdp_release_x11(sys->vdp);
    av_freep(hwctx);
    free(sys);
}

vlc_module_begin()
    set_description(N_("VDPAU video decoder"))
    set_capability("hw decoder", 100)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, Close)
    add_shortcut("vdpau")
vlc_module_end()
