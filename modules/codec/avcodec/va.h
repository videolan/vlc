/*****************************************************************************
 * va.h: Video Acceleration API for avcodec
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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

#include "avcommon_compat.h"

#ifndef VLC_AVCODEC_VA_H
#define VLC_AVCODEC_VA_H 1

typedef struct vlc_va_t vlc_va_t;
typedef struct vlc_va_sys_t vlc_va_sys_t;

struct vlc_va_t {
    struct vlc_common_members obj;

    vlc_va_sys_t *sys;
    int  (*get)(vlc_va_t *, picture_t *pic, uint8_t **surface);
};

/**
 * Determines the VLC video chroma value for a pair of hardware acceleration
 * PixelFormat and software PixelFormat.
 * @param hwfmt the hardware acceleration pixel format
 * @param swfmt the software pixel format
 * @return a VLC chroma value, or 0 on error.
 */
vlc_fourcc_t vlc_va_GetChroma(enum PixelFormat hwfmt, enum PixelFormat swfmt);

/**
 * Creates an accelerated video decoding back-end for libavcodec.
 * @param obj parent VLC object
 * @param fmt VLC format of the content to decode
 * @return a new VLC object on success, NULL on error.
 */
vlc_va_t *vlc_va_New(vlc_object_t *obj, AVCodecContext *,
                     enum PixelFormat, const es_format_t *fmt,
                     void *p_sys);

/**
 * Get a hardware video surface for a libavcodec frame.
 * The surface will be used as output for the hardware decoder, and possibly
 * also as a reference frame to decode other surfaces.
 *
 * The type of the surface depends on the hardware pixel format:
 * AV_PIX_FMT_D3D11VA_VLD - ID3D11VideoDecoderOutputView*
 * AV_PIX_FMT_DXVA2_VLD   - IDirect3DSurface9*
 * AV_PIX_FMT_VDPAU       - VdpVideoSurface
 * AV_PIX_FMT_VAAPI_VLD   - VASurfaceID
 *
 * @param pic pointer to VLC picture containing the surface [IN/OUT]
 * @param surface pointer to the AVFrame data[0] and data[3] pointers [OUT]
 *
 * @note This function needs not be reentrant.
 *
 * @return VLC_SUCCESS on success, otherwise an error code.
 */
static inline int vlc_va_Get(vlc_va_t *va, picture_t *pic, uint8_t **surface)
{
    return va->get(va, pic, surface);
}

/**
 * Destroys a libavcodec hardware acceleration back-end.
 * All allocated surfaces shall have been released beforehand.
 */
void vlc_va_Delete(vlc_va_t *, void **);

#endif
