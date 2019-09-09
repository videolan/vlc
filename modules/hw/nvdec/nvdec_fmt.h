/*****************************************************************************
 * nvdec_fmt.h : NVDEC common code
 *****************************************************************************
 * Copyright © 2019 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
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

#ifndef VLC_VIDEOCHROMA_NVDEC_FMT_H_
#define VLC_VIDEOCHROMA_NVDEC_FMT_H_

#include <ffnvcodec/dynlink_loader.h>

static inline bool is_nvdec_opaque(vlc_fourcc_t fourcc)
{
    return fourcc == VLC_CODEC_NVDEC_OPAQUE ||
           fourcc == VLC_CODEC_NVDEC_OPAQUE_10B ||
           fourcc == VLC_CODEC_NVDEC_OPAQUE_16B;
}

/* for VLC_CODEC_NVDEC_OPAQUE / VLC_CODEC_NVDEC_OPAQUE_16B */
typedef struct
{
    picture_context_t ctx;
    CUdeviceptr  devidePtr;
    unsigned int bufferPitch;
    unsigned int bufferHeight;
} pic_context_nvdec_t;

#endif /* include-guard */
