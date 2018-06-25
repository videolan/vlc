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

# if _WIN32_WINNT < _WIN32_WINNT_VISTA
/* d3d11 needs Vista support */
#  undef _WIN32_WINNT
#  define _WIN32_WINNT _WIN32_WINNT_VISTA
# endif

#include <vlc_common.h>

#include <libavcodec/avcodec.h>
#include "va.h"

#include <unknwn.h>
#include <stdatomic.h>

#include "va_surface_internal.h"

typedef struct input_list_t {
    void (*pf_release)(struct input_list_t *);
    GUID *list;
    unsigned count;
} input_list_t;

#define MAX_SURFACE_COUNT (64)
typedef struct
{
    va_pool_t             va_pool;
    bool                  can_extern_pool;

    /* for pre allocation */
    D3D_DecoderSurface     *hw_surface[MAX_SURFACE_COUNT];

    /* Video service */
    GUID                   input;
    D3D_DecoderDevice      *d3ddec;

    /* Video decoder */
    D3D_DecoderType        *decoder;

    /**
     * Read the list of possible input GUIDs
     */
    int (*pf_get_input_list)(vlc_va_t *, input_list_t *);
    /**
     * Find a suitable decoder configuration for the input and set the
     * internal state to use that output
     */
    int (*pf_setup_output)(vlc_va_t *, const GUID *input, const video_format_t *fmt);

} directx_sys_t;

int directx_va_Open(vlc_va_t *, directx_sys_t *);
void directx_va_Close(vlc_va_t *, directx_sys_t *);
int directx_va_Setup(vlc_va_t *, directx_sys_t *, const AVCodecContext *avctx, const es_format_t *, int flag_xbox);
char *directx_va_GetDecoderName(const GUID *guid);
bool directx_va_canUseDecoder(vlc_va_t *, UINT VendorId, UINT DeviceId, const GUID *pCodec);

#endif /* AVCODEC_DIRECTX_VA_H */
