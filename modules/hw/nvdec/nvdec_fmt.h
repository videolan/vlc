/*****************************************************************************
 * nvdec_fmt.h : NVDEC common shared code
 *****************************************************************************
 * Copyright © 2019-2023 VLC authors, VideoLAN and VideoLabs
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

#include <vlc_fourcc.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline vlc_fourcc_t NVDECToVlcChroma(vlc_fourcc_t chroma)
{
    switch (chroma)
    {
    case VLC_CODEC_NVDEC_OPAQUE:
        return VLC_CODEC_NV12;
    case VLC_CODEC_NVDEC_OPAQUE_10B:
        return VLC_CODEC_P010;
    case VLC_CODEC_NVDEC_OPAQUE_16B:
        return VLC_CODEC_P016;
    case VLC_CODEC_NVDEC_OPAQUE_444:
        return VLC_CODEC_I444;
    case VLC_CODEC_NVDEC_OPAQUE_444_16B:
        return VLC_CODEC_I444_16L;
    default:
        return 0;
    }
}

static inline bool is_nvdec_opaque(vlc_fourcc_t fourcc)
{
    return NVDECToVlcChroma(fourcc) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* include-guard */
