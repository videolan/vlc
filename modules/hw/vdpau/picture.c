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
#include <assert.h>
#include <vlc_common.h>
#include <vlc_picture.h>
#include "vlc_vdpau.h"

#pragma GCC visibility push(default)

static void SurfaceDestroy(void *opaque)
{
    vlc_vdp_video_field_t *field = opaque;
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

VdpStatus vlc_vdp_video_attach(vdp_t *vdp, VdpVideoSurface surface,
                               picture_t *pic)
{
    vlc_vdp_video_field_t *field = malloc(sizeof (*field));
    vlc_vdp_video_frame_t *frame = malloc(sizeof (*frame));

    if (unlikely(field == NULL || frame == NULL))
    {
        free(frame);
        free(field);
        vdp_video_surface_destroy(vdp, surface);
        return VDP_STATUS_RESOURCES;
    }

    assert(pic->context == NULL);
    pic->context = field;

    field->destroy = SurfaceDestroy;
    field->frame = frame;
    field->structure = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    field->sharpen = 0.f;

    atomic_init(&frame->refs, 1);
    frame->surface = surface;
    frame->vdp = vdp_hold_x11(vdp, &frame->device);
    return VDP_STATUS_OK;
}

VdpStatus vlc_vdp_video_copy(picture_t *restrict dst, picture_t *restrict src)
{
    vlc_vdp_video_field_t *fold = src->context;
    vlc_vdp_video_frame_t *frame = fold->frame;
    vlc_vdp_video_field_t *fnew = malloc(sizeof (*fnew));
    if (unlikely(fnew == NULL))
        return VDP_STATUS_RESOURCES;

    assert(dst->context == NULL);
    dst->context = fnew;

    fnew->destroy = SurfaceDestroy;
    fnew->frame = frame;
    fnew->structure = fold->structure;
    fnew->sharpen = fold->sharpen;

    atomic_fetch_add(&frame->refs, 1);
    return VDP_STATUS_OK;
}
