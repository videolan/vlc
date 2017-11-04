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
#include "vlc_vdpau.h"

#pragma GCC visibility push(default)

static_assert(offsetof (vlc_vdp_video_field_t, context) == 0,
              "Cast assumption failure");

static void SurfaceDestroy(struct picture_context_t *ctx)
{
    vlc_vdp_video_field_t *field = (vlc_vdp_video_field_t *)ctx;
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

static picture_context_t *SurfaceCopy(picture_context_t *ctx)
{
    vlc_vdp_video_field_t *fold = (vlc_vdp_video_field_t *)ctx;
    vlc_vdp_video_frame_t *frame = fold->frame;
    vlc_vdp_video_field_t *fnew = malloc(sizeof (*fnew));
    if (unlikely(fnew == NULL))
        return NULL;

    fnew->context.destroy = SurfaceDestroy;
    fnew->context.copy = SurfaceCopy;
    fnew->frame = frame;
    fnew->structure = fold->structure;
    fnew->procamp = fold->procamp;
    fnew->sharpen = fold->sharpen;

    atomic_fetch_add(&frame->refs, 1);
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

    field->context.destroy = SurfaceDestroy;
    field->context.copy = SurfaceCopy;
    field->frame = frame;
    field->structure = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    field->procamp = procamp_default;
    field->sharpen = 0.f;

    atomic_init(&frame->refs, 1);
    frame->surface = surface;
    frame->vdp = vdp_hold_x11(vdp, &frame->device);
    return field;
}

VdpStatus vlc_vdp_video_attach(vdp_t *vdp, VdpVideoSurface surface,
                               picture_t *pic)
{
    vlc_vdp_video_field_t *field = vlc_vdp_video_create(vdp, surface);
    if (unlikely(field == NULL))
        return VDP_STATUS_RESOURCES;

    assert(pic->format.i_chroma == VLC_CODEC_VDPAU_VIDEO_420
        || pic->format.i_chroma == VLC_CODEC_VDPAU_VIDEO_422
        || pic->format.i_chroma == VLC_CODEC_VDPAU_VIDEO_444);
    assert(pic->context == NULL);
    pic->context = &field->context;
    return VDP_STATUS_OK;
}
