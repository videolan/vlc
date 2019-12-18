/*****************************************************************************
 * va.h: Video Acceleration API for avcodec
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
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

#ifndef VLC_AVCODEC_VA_H
#define VLC_AVCODEC_VA_H 1

#include "avcommon_compat.h"
#include <libavutil/pixdesc.h>

typedef struct vlc_va_t vlc_va_t;
typedef struct vlc_va_sys_t vlc_va_sys_t;
typedef struct vlc_decoder_device vlc_decoder_device;
typedef struct vlc_video_context vlc_video_context;

struct vlc_va_operations {
    int (*get)(vlc_va_t *, picture_t *pic, uint8_t **surface);
    void (*close)(vlc_va_t *);
};

struct vlc_va_t {
    struct vlc_object_t obj;

    vlc_va_sys_t *sys;
    const struct vlc_va_operations *ops;
};

typedef int (*vlc_va_open)(vlc_va_t *, AVCodecContext *,
                           enum PixelFormat hwfmt, const AVPixFmtDescriptor *,
                           const es_format_t *, vlc_decoder_device *,
                           video_format_t *, vlc_video_context **);

#define set_va_callback(activate, priority) \
    { \
        vlc_va_open open__ = activate; \
        (void) open__; \
        set_callback(activate) \
    } \
    set_capability( "hw decoder", priority )

/**
 * Determines whether the hardware acceleration PixelFormat can be used to
 * decode pixels similar to the software PixelFormat.
 * @param hwfmt the hardware acceleration pixel format
 * @param swfmt the software pixel format
 * @return true if the hardware acceleration should be supported
 */
bool vlc_va_MightDecode(enum PixelFormat hwfmt, enum PixelFormat swfmt);

/**
 * Creates an accelerated video decoding back-end for libavcodec.
 * @param obj parent VLC object
 * @param fmt VLC format of the content to decode
 * @return a new VLC object on success, NULL on error.
 */
vlc_va_t *vlc_va_New(vlc_object_t *obj, AVCodecContext *,
                     enum PixelFormat hwfmt, const AVPixFmtDescriptor *,
                     const es_format_t *fmt, vlc_decoder_device *device,
                     video_format_t *, vlc_video_context **vtcx_out);

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
    return va->ops->get(va, pic, surface);
}

/**
 * Destroys a libavcodec hardware acceleration back-end.
 * All allocated surfaces shall have been released beforehand.
 */
void vlc_va_Delete(vlc_va_t *);

#endif
