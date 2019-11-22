/*****************************************************************************
 * picture.c: VDPAU instance management for VLC
 *****************************************************************************
 * Copyright (C) 2013 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>
#include "vlc_vdpau.h"

#pragma GCC visibility push(default)

static_assert(offsetof (vlc_vdp_video_field_t, context) == 0,
              "Cast assumption failure");

static void VideoSurfaceDestroy(struct picture_context_t *ctx)
{
    vlc_vdp_video_field_t *field = container_of(ctx, vlc_vdp_video_field_t,
                                                context);
    vlc_vdp_video_frame_t *frame = field->frame;
    VdpStatus err;

    /* Destroy field-specific infos */
    free(field);

    if (atomic_fetch_sub(&frame->refs, 1) != 1)
        return;

    /* Destroy frame (video surface) */
    err = vdp_video_surface_destroy(frame->vdp, frame->surface);
    if (err != VDP_STATUS_OK)
        fprintf(stderr, "video surface destruction failure: %s\n",
                vdp_get_error_string(frame->vdp, err));
    vdp_release_x11(frame->vdp);
    free(frame);
}

static picture_context_t *VideoSurfaceCopy(picture_context_t *ctx)
{
    vlc_vdp_video_field_t *fold = container_of(ctx, vlc_vdp_video_field_t,
                                               context);
    vlc_vdp_video_field_t *fnew = malloc(sizeof (*fnew));
    if (unlikely(fnew == NULL))
        return NULL;

    *fnew = *fold;

    atomic_fetch_add(&fold->frame->refs, 1);
    return &fnew->context;
}

static const VdpProcamp procamp_default =
{
    .struct_version = VDP_PROCAMP_VERSION,
    .brightness = 0.f,
    .contrast = 1.f,
    .saturation = 1.f,
    .hue = 0.f,
};

vlc_vdp_video_field_t *vlc_vdp_video_create(vdp_t *vdp,
                                            VdpVideoSurface surface)
{
    vlc_vdp_video_field_t *field = malloc(sizeof (*field));
    vlc_vdp_video_frame_t *frame = malloc(sizeof (*frame));

    if (unlikely(field == NULL || frame == NULL))
    {
        free(frame);
        free(field);
        return NULL;
    }

    field->context = (picture_context_t) {
        VideoSurfaceDestroy, VideoSurfaceCopy,
        NULL /*TODO*/
    };
    field->frame = frame;
    field->structure = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    field->procamp = procamp_default;
    field->sharpen = 0.f;

    atomic_init(&frame->refs, 1);
    frame->surface = surface;
    frame->vdp = vdp_hold_x11(vdp, &frame->device);
    return field;
}

picture_context_t *VideoSurfaceCloneWithContext(picture_context_t *src_ctx)
{
    picture_context_t *dst_ctx = VideoSurfaceCopy(src_ctx);
    if (unlikely(dst_ctx == NULL))
        return NULL;
    vlc_video_context_Hold(dst_ctx->vctx);
    return dst_ctx;
}

VdpStatus vlc_vdp_video_attach(vdp_t *vdp, VdpVideoSurface surface,
                               vlc_video_context *vctx, picture_t *pic)
{
    vlc_vdp_video_field_t *field = vlc_vdp_video_create(vdp, surface);
    if (unlikely(field == NULL))
        return VDP_STATUS_RESOURCES;

    field->context.destroy = VideoSurfaceDestroy;
    field->context.copy = VideoSurfaceCloneWithContext;
    field->context.vctx = vlc_video_context_Hold(vctx);

    assert(pic->format.i_chroma == VLC_CODEC_VDPAU_VIDEO_420
        || pic->format.i_chroma == VLC_CODEC_VDPAU_VIDEO_422
        || pic->format.i_chroma == VLC_CODEC_VDPAU_VIDEO_444);
    assert(pic->context == NULL);
    pic->context = &field->context;
    return VDP_STATUS_OK;
}

static void vlc_vdp_output_surface_destroy(picture_t *pic)
{
    vlc_vdp_output_surface_t *sys = pic->p_sys;

    vdp_output_surface_destroy(sys->vdp, sys->surface);
    vdp_release_x11(sys->vdp);
    free(sys);
}

static
picture_t *vlc_vdp_output_surface_create(vdpau_decoder_device_t *vdpau_dev,
                                         VdpRGBAFormat rgb_fmt,
                                         const video_format_t *restrict fmt)
{
    vlc_vdp_output_surface_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return NULL;

    sys->vdp = vdp_hold_x11(vdpau_dev->vdp, &sys->device);
    sys->gl_nv_surface = 0;

    VdpStatus err = vdp_output_surface_create(vdpau_dev->vdp, vdpau_dev->device,
        rgb_fmt,
        fmt->i_visible_width, fmt->i_visible_height, &sys->surface);
    if (err != VDP_STATUS_OK)
    {
error:
        vdp_release_x11(vdpau_dev->vdp);
        free(sys);
        return NULL;
    }

    picture_resource_t res = {
        .p_sys = sys,
        .pf_destroy = vlc_vdp_output_surface_destroy,
    };

    picture_t *pic = picture_NewFromResource(fmt, &res);
    if (unlikely(pic == NULL))
    {
        vdp_output_surface_destroy(vdpau_dev->vdp, sys->surface);
        goto error;
    }
    return pic;
}

picture_pool_t *vlc_vdp_output_pool_create(vdpau_decoder_device_t *vdpau_dev,
                                           VdpRGBAFormat rgb_fmt,
                                           const video_format_t *restrict fmt,
                                           unsigned requested_count)
{
    picture_t *pics[requested_count];
    unsigned count = 0;

    while (count < requested_count)
    {
        pics[count] = vlc_vdp_output_surface_create(vdpau_dev, rgb_fmt, fmt);
        if (pics[count] == NULL)
            break;
        count++;
    }

    if (count == 0)
        return NULL;

    picture_pool_t *pool = picture_pool_New(count, pics);
    if (unlikely(pool == NULL))
        while (count > 0)
            picture_Release(pics[--count]);
    return pool;
}
