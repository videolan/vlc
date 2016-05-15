/*****************************************************************************
 * directx_va.h: DirectX Generic Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * Copyright (C) 2015 Steve Lhomme
 * $Id$
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

#ifndef AVCODEC_DIRECTX_VA_H
#define AVCODEC_DIRECTX_VA_H

# if _WIN32_WINNT < 0x600
/* d3d11 needs Vista support */
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x600
# endif

#include <vlc_common.h>

#include <libavcodec/avcodec.h>
#include "va.h"

#include <unknwn.h>

/* */
typedef struct {
    int                refcount;
    unsigned int       order;
    vlc_mutex_t        *p_lock;
    picture_t          *p_pic;
} vlc_va_surface_t;

typedef struct input_list_t {
    void (*pf_release)(struct input_list_t *);
    GUID *list;
    unsigned count;
} input_list_t;

#define MAX_SURFACE_COUNT (64)
typedef struct
{
    int          codec_id;
    int          width;
    int          height;

    /* DLL */
    HINSTANCE             hdecoder_dll;
    const TCHAR           *psz_decoder_dll;

    /* Direct3D */
    IUnknown              *d3ddev;

    /* Video service */
    GUID                   input;
    IUnknown               *d3ddec;

    /* Video decoder */
    IUnknown               *decoder;

    /* */
    int          surface_count;
    int          surface_order;
    int          surface_width;
    int          surface_height;

    int          thread_count;

    vlc_mutex_t      surface_lock;
    vlc_va_surface_t surface[MAX_SURFACE_COUNT];
    IUnknown         *hw_surface[MAX_SURFACE_COUNT];

    /**
     * Check that the decoder device is still available
     */
    int (*pf_check_device)(vlc_va_t *);

    int (*pf_create_device)(vlc_va_t *);
    void (*pf_destroy_device)(vlc_va_t *);

    int (*pf_create_device_manager)(vlc_va_t *);
    void (*pf_destroy_device_manager)(vlc_va_t *);

    int (*pf_create_video_service)(vlc_va_t *);
    void (*pf_destroy_video_service)(vlc_va_t *);

    /**
     * Read the list of possible input GUIDs
     */
    int (*pf_get_input_list)(vlc_va_t *, input_list_t *);
    /**
     * Find a suitable decoder configuration for the input and set the
     * internal state to use that output
     */
    int (*pf_setup_output)(vlc_va_t *, const GUID *input, const video_format_t *fmt);

    /**
     * Create the DirectX surfaces in hw_surface and the decoder in decoder
     */
    int (*pf_create_decoder_surfaces)(vlc_va_t *, int codec_id,
                                      const video_format_t *fmt, bool b_threading);
    /**
     * Destroy resources allocated with the surfaces except from hw_surface objects
     */
    void (*pf_destroy_surfaces)(vlc_va_t *);
    /**
     * Set the avcodec hw context after the decoder is created
     */
    void (*pf_setup_avcodec_ctx)(vlc_va_t *);
    /**
     * @brief pf_alloc_surface_pic
     * @param fmt
     * @return
     */
    picture_t *(*pf_alloc_surface_pic)(vlc_va_t *, const video_format_t *, unsigned);

} directx_sys_t;

int directx_va_Open(vlc_va_t *, directx_sys_t *, AVCodecContext *ctx, const es_format_t *fmt, bool b_dll);
void directx_va_Close(vlc_va_t *, directx_sys_t *);
int directx_va_Setup(vlc_va_t *, directx_sys_t *, AVCodecContext *avctx);
int directx_va_Get(vlc_va_t *, directx_sys_t *, picture_t *pic, uint8_t **data);
void directx_va_Release(void *opaque, uint8_t *data);
char *directx_va_GetDecoderName(const GUID *guid);

#endif /* AVCODEC_DIRECTX_VA_H */
