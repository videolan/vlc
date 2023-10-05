/**
 * @file fourcc.c
 * @brief DRM FourCC's
 */
/*****************************************************************************
 * Copyright © 2022 Rémi Denis-Courmont
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

#include <stdint.h>
#ifndef HAVE_LIBDRM
# include <drm/drm_fourcc.h>
#else
# include <drm_fourcc.h>
#endif
#include <vlc_common.h>
#include <vlc_es.h>
#include "vlc_drm.h"

/*
 For reference, the last time these tables were updated, the following DRM
 pixel formats had no equivalents in VLC, and there are no urges to add them:
   DRM_FORMAT_XRGB16161616F
   DRM_FORMAT_XBGR16161616F
   DRM_FORMAT_ARGB16161616F
   DRM_FORMAT_ABGR16161616F
   DRM_FORMAT_ARGB4444
   DRM_FORMAT_ABGR4444
   DRM_FORMAT_RGBA4444
   DRM_FORMAT_BGRA4444
   DRM_FORMAT_ARGB1555
   DRM_FORMAT_ABGR1555
   DRM_FORMAT_RGBA5551
   DRM_FORMAT_BGRA5551
   DRM_FORMAT_XRGB2101010
   DRM_FORMAT_XBGR2101010
   DRM_FORMAT_RGBX1010102
   DRM_FORMAT_BGRX1010102
   DRM_FORMAT_ARGB2101010
   DRM_FORMAT_RGBA1010102
   DRM_FORMAT_BGRA1010102
   DRM_FORMAT_AXBXGXRX106106106106
   DRM_FORMAT_XYUV8888
   DRM_FORMAT_VUY888 (VLC_CODEC_V308, not a pixel format)
   DRM_FORMAT_Y210 (*not* the same as VLC_CODEC_Y210)
   DRM_FORMAT_Y212
   DRM_FORMAT_Y216
   DRM_FORMAT_Y410 (*not* the same as VLC_CODEC_Y410)
   DRM_FORMAT_Y412
   DRM_FORMAT_Y416
   DRM_FORMAT_XVYU2101010
   DRM_FORMAT_XVYU12_16161616
   DRM_FORMAT_XVYU16161616
   DRM_FORMAT_P210
   DRM_FORMAT_P012
   DRM_FORMAT_Q410 (*not* the same as VLC_CODEC_I444_10L, MSB)
   DRM_FORMAT_Q401
   DRM_FORMAT_YVU411
   DRM_FORMAT_YVU422
   DRM_FORMAT_YVU444

 These DRM formats are semiplanar RGB/A:
   DRM_FORMAT_XRGB8888_A8
   DRM_FORMAT_XBGR8888_A8
   DRM_FORMAT_RGBX8888_A8
   DRM_FORMAT_BGRX8888_A8
   DRM_FORMAT_RGB888_A8
   DRM_FORMAT_BGR888_A8
   DRM_FORMAT_RGB565_A8
   DRM_FORMAT_BGR565_A8

 These DRM formats are used for planes within a multiplanar buffer:
   DRM_FORMAT_C8
   DRM_FORMAT_R8
   DRM_FORMAT_R16
   DRM_FORMAT_RG88
   DRM_FORMAT_GR88
   DRM_FORMAT_RG1616
   DRM_FORMAT_GR1616

 These DRM formats are not usable linearly, meaning they can only be used for
 tiled opaque buffers. VLC cannot define them as non-opaque formats:
   DRM_FORMAT_VUY101010
   DRM_FORMAT_Y0L0
   DRM_FORMAT_X0L0
   DRM_FORMAT_Y0L2
   DRM_FORMAT_X0L2
   DRM_FORMAT_YUV420_8BIT
   DRM_FORMAT_YUV420_10BIT
   DRM_FORMAT_NV15

 */

static const struct {
    uint32_t drm_fourcc;
    vlc_fourcc_t vlc_fourcc;
} fourcc_list[] = {
    /* Beware: DRM uses little endian while VLC uses big endian */
    /* RGBA formats */
    { DRM_FORMAT_ARGB8888, VLC_CODEC_BGRA },
    { DRM_FORMAT_ABGR8888, VLC_CODEC_RGBA },
    { DRM_FORMAT_BGRA8888, VLC_CODEC_ARGB },
    { DRM_FORMAT_RGBA8888, VLC_CODEC_ABGR },
#ifndef WORDS_BIGENDIAN
    { DRM_FORMAT_ABGR2101010, VLC_CODEC_RGBA10LE },
#endif

    /* Packed RGB+x formats */
    { DRM_FORMAT_XRGB8888, VLC_CODEC_BGRX },
    { DRM_FORMAT_XBGR8888, VLC_CODEC_RGBX },
    { DRM_FORMAT_RGBX8888, VLC_CODEC_XBGR },
    { DRM_FORMAT_BGRX8888, VLC_CODEC_XRGB },

    /* Packed RGB24 formats */
    { DRM_FORMAT_RGB888, VLC_CODEC_BGR24 },
    { DRM_FORMAT_BGR888, VLC_CODEC_RGB24 },

    /* Packed RGB16 formats */
    { DRM_FORMAT_RGB565, VLC_CODEC_RGB565LE },
    { DRM_FORMAT_BGR565, VLC_CODEC_BGR565LE },
    { DRM_FORMAT_RGB565 | DRM_FORMAT_BIG_ENDIAN, VLC_CODEC_RGB565BE },
    { DRM_FORMAT_BGR565 | DRM_FORMAT_BIG_ENDIAN, VLC_CODEC_BGR565BE },

    /* 16-bit-padded 15-bit RGB */
    { DRM_FORMAT_XRGB1555, VLC_CODEC_RGB555LE },
    { DRM_FORMAT_XBGR1555, VLC_CODEC_BGR555LE },
    { DRM_FORMAT_XRGB1555 | DRM_FORMAT_BIG_ENDIAN, VLC_CODEC_RGB555BE },
    // { DRM_FORMAT_RGBX5551, 0 },
    // { DRM_FORMAT_BGRX5551, 0 },

    /* 8-bit RGB */
    { DRM_FORMAT_RGB332,   VLC_CODEC_RGB332 },
    { DRM_FORMAT_BGR233,   VLC_CODEC_BGR233 },

    /* Packed YUV formats */
    /* DRM uses big-endian for YUY2, otherwise little endian. */
    { DRM_FORMAT_YUYV,     VLC_CODEC_YUYV },
    { DRM_FORMAT_YVYU,     VLC_CODEC_YVYU },
    { DRM_FORMAT_UYVY,     VLC_CODEC_UYVY },
    { DRM_FORMAT_VYUY,     VLC_CODEC_VYUY },

    /* Packed YUVA */
    { DRM_FORMAT_AYUV,     VLC_CODEC_VUYA },

    /* Semiplanar YUV */
    { DRM_FORMAT_NV12,     VLC_CODEC_NV12 },
    { DRM_FORMAT_NV21,     VLC_CODEC_NV21 },
    { DRM_FORMAT_NV16,     VLC_CODEC_NV16 },
    { DRM_FORMAT_NV61,     VLC_CODEC_NV61 },
    { DRM_FORMAT_NV24,     VLC_CODEC_NV24 },
    { DRM_FORMAT_NV42,     VLC_CODEC_NV42 },
    { DRM_FORMAT_P010,     VLC_CODEC_P010 },
    { DRM_FORMAT_P016,     VLC_CODEC_P016 },

    /* Planar YUV */
    { DRM_FORMAT_YUV410,   VLC_CODEC_I410 },
    { DRM_FORMAT_YUV411,   VLC_CODEC_I411 },
    { DRM_FORMAT_YUV420,   VLC_CODEC_I420 },
    { DRM_FORMAT_YVU420,   VLC_CODEC_YV12 },
    { DRM_FORMAT_YUV422,   VLC_CODEC_I422 },
    { DRM_FORMAT_YUV444,   VLC_CODEC_I444 },
};

uint_fast32_t vlc_drm_fourcc(vlc_fourcc_t vlc_fourcc)
{
    for (size_t i = 0; i < ARRAY_SIZE(fourcc_list); i++)
        if (fourcc_list[i].vlc_fourcc == vlc_fourcc)
            return fourcc_list[i].drm_fourcc;

    return DRM_FORMAT_INVALID;
}

vlc_fourcc_t vlc_fourcc_drm(uint_fast32_t drm_fourcc)
{
    for (size_t i = 0; i < ARRAY_SIZE(fourcc_list); i++)
        if (fourcc_list[i].drm_fourcc == drm_fourcc)
            return fourcc_list[i].vlc_fourcc;

    return 0;
}
