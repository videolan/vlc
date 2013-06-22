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
    vlc_vdp_video_t *ctx = opaque;
    VdpStatus err;

    err = vdp_video_surface_destroy(ctx->vdp, ctx->surface);
    if (err != VDP_STATUS_OK)
        fprintf(stderr, "video surface destruction failure: %s\n",
                vdp_get_error_string(ctx->vdp, err));
    vdp_release_x11(ctx->vdp);
    free(ctx);
}

VdpStatus vlc_vdp_video_attach(vdp_t *vdp, VdpVideoSurface surface,
                               picture_t *pic)
{
    vlc_vdp_video_t *ctx = malloc(sizeof (*ctx));
    if (unlikely(ctx == NULL))
    {
        vdp_video_surface_destroy(vdp, surface);
        return VDP_STATUS_RESOURCES;
    }

    ctx->destroy = SurfaceDestroy;
    ctx->surface = surface;
    ctx->vdp = vdp_hold_x11(vdp, &ctx->device);
    assert(pic->context == NULL);
    pic->context = ctx;
    return VDP_STATUS_OK;
}
