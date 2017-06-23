/*****************************************************************************
 * va_surface_internal.h: libavcodec Generic Video Acceleration helpers
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

#ifndef AVCODEC_VA_SURFACE_INTERNAL_H
#define AVCODEC_VA_SURFACE_INTERNAL_H

#include "va_surface.h"

#include <libavcodec/avcodec.h>
#include "va.h"

#include <stdatomic.h>

/* */
typedef struct vlc_va_surface_t vlc_va_surface_t;

#define MAX_SURFACE_COUNT (64)
typedef struct
{
    /* */
    unsigned     surface_count;
    int          surface_width;
    int          surface_height;

    struct va_pic_context  *surface[MAX_SURFACE_COUNT];

    int (*pf_create_device)(vlc_va_t *);
    void (*pf_destroy_device)(vlc_va_t *);

    int (*pf_create_device_manager)(vlc_va_t *);
    void (*pf_destroy_device_manager)(vlc_va_t *);

    int (*pf_create_video_service)(vlc_va_t *);
    void (*pf_destroy_video_service)(vlc_va_t *);

    /**
     * Create the DirectX surfaces in hw_surface and the decoder in decoder
     */
    int (*pf_create_decoder_surfaces)(vlc_va_t *, int codec_id,
                                      const video_format_t *fmt,
                                      unsigned surface_count);
    /**
     * Destroy resources allocated with the surfaces and the associated decoder
     */
    void (*pf_destroy_surfaces)(vlc_va_t *);
    /**
     * Set the avcodec hw context after the decoder is created
     */
    void (*pf_setup_avcodec_ctx)(vlc_va_t *);

    /**
     * Create a new context for the surface being acquired
     */
    struct va_pic_context* (*pf_new_surface_context)(vlc_va_t *, int surface_index);

} va_pool_t;

int va_pool_Open(vlc_va_t *, va_pool_t *);
void va_pool_Close(vlc_va_t *va, va_pool_t *);
int va_pool_SetupDecoder(vlc_va_t *, va_pool_t *, const AVCodecContext *, unsigned count, int alignment);
int va_pool_SetupSurfaces(vlc_va_t *, va_pool_t *, unsigned count);
int va_pool_Get(va_pool_t *, picture_t *);
void va_surface_AddRef(vlc_va_surface_t *surface);
void va_surface_Release(vlc_va_surface_t *surface);

#endif /* AVCODEC_VA_SURFACE_INTERNAL_H */
