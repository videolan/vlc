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
   DRM_FORMAT_RGBA8888 (VLC_CODEC_ABGR, not defined)
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

/* RGB (no alpha) formats.
 * For historical reasons, VLC uses same FourCC with different masks. */
static const struct {
    uint32_t drm_fourcc;
    vlc_fourcc_t vlc_fourcc;
    uint32_t red; /**< Little endian red mask */
    uint32_t green; /**< Little endian green mask */
    uint32_t blue; /**< Little endian blue mask */
} rgb_fourcc_list[] = {
    /* 8-bit RGB */
    { DRM_FORMAT_RGB332,   VLC_CODEC_RGB8, 0xD0, 0x16, 0x03 },
    { DRM_FORMAT_BGR233,   VLC_CODEC_RGB8, 0x07, 0x28, 0xC0 },
#ifdef WORDS_BIGENDIAN
    /* 16-bit-padded 12-bit RGB */
    { DRM_FORMAT_XRGB4444, VLC_CODEC_RGB12, 0x000F, 0xF000, 0x0F00 },
    { DRM_FORMAT_XBGR4444, VLC_CODEC_RGB12, 0x0F00, 0xF000, 0x000F },
    { DRM_FORMAT_RGBX4444, VLC_CODEC_RGB12, 0x00F0, 0x000F, 0xF000 },
    { DRM_FORMAT_BGRX4444, VLC_CODEC_RGB12, 0xF000, 0x000F, 0x00F0 },
    /* 24-bit RGB */
    { DRM_FORMAT_RGB888,   VLC_CODEC_RGB24, 0x0000FF, 0x00FF00, 0xFF0000 },
    { DRM_FORMAT_BGR888,   VLC_CODEC_RGB24, 0xFF0000, 0x00FF00, 0x0000FF },
    /* 32-bit-padded 24-bit RGB */
    { DRM_FORMAT_XRGB8888, VLC_CODEC_RGB32,
                                          0x0000FF00, 0x00FF0000, 0xFF000000 },
    { DRM_FORMAT_XBGR8888, VLC_CODEC_RGB32,
                                          0xFF000000, 0x00FF0000, 0x0000FF00 },
    { DRM_FORMAT_RGBX8888, VLC_CODEC_RGB32,
                                          0x000000FF, 0x0000FF00, 0x00FF0000 },
    { DRM_FORMAT_BGRX8888, VLC_CODEC_RGB32,
                                          0x00FF0000, 0x0000FF00, 0x000000FF },
#else
    /* 16-bit-padded 12-bit RGB */
    { DRM_FORMAT_XRGB4444, VLC_CODEC_RGB12, 0x0F00, 0x00F0, 0x000F },
    { DRM_FORMAT_XBGR4444, VLC_CODEC_RGB12, 0x000F, 0x00F0, 0x0F00 },
    { DRM_FORMAT_RGBX4444, VLC_CODEC_RGB12, 0xF000, 0x0F00, 0x00F0 },
    { DRM_FORMAT_BGRX4444, VLC_CODEC_RGB12, 0x00F0, 0x0F00, 0xF000 },
    /* 16-bit-padded 15-bit RGB */
    { DRM_FORMAT_XRGB1555, VLC_CODEC_RGB15, 0x7C00, 0x03E0, 0x001F },
    { DRM_FORMAT_XBGR1555, VLC_CODEC_RGB15, 0x001F, 0x03E0, 0x7C00 },
    { DRM_FORMAT_RGBX5551, VLC_CODEC_RGB15, 0xF800, 0x07C0, 0x003E },
    { DRM_FORMAT_BGRX5551, VLC_CODEC_RGB15, 0x003E, 0x07C0, 0xF800 },
    /* 16-bit RGB */
    { DRM_FORMAT_RGB565,   VLC_CODEC_RGB16, 0xF800, 0x07E0, 0x001F },
    { DRM_FORMAT_BGR565,   VLC_CODEC_RGB16, 0x001F, 0x07E0, 0xF800 },
    /* 24-bit RGB */
    { DRM_FORMAT_RGB888,   VLC_CODEC_RGB24, 0xFF0000, 0x00FF00, 0x0000FF },
    { DRM_FORMAT_BGR888,   VLC_CODEC_RGB24, 0x0000FF, 0x00FF00, 0xFF0000 },
    /* 32-bit-padded 24-bit RGB */
    { DRM_FORMAT_XRGB8888, VLC_CODEC_RGB32,
                                          0x00FF0000, 0x0000FF00, 0x000000FF },
    { DRM_FORMAT_XBGR8888, VLC_CODEC_RGB32,
                                          0x000000FF, 0x0000FF00, 0x00FF0000 },
    { DRM_FORMAT_RGBX8888, VLC_CODEC_RGB32,
                                          0xFF000000, 0x00FF0000, 0x0000FF00 },
    { DRM_FORMAT_BGRX8888, VLC_CODEC_RGB32,
                                          0x0000FF00, 0x00FF0000, 0xFF000000 },
#endif
};

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
    { DRM_FORMAT_ABGR2101010, VLC_CODEC_RGBA10 },
#endif

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
    { DRM_FORMAT_YVU410,   VLC_CODEC_YV9 },
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

uint_fast32_t vlc_drm_format(const video_format_t *restrict fmt)
{
    uint_fast32_t drm_fourcc = vlc_drm_fourcc(fmt->i_chroma);
    if (drm_fourcc != DRM_FORMAT_INVALID)
        return drm_fourcc;

    for (size_t i = 0; i < ARRAY_SIZE(rgb_fourcc_list); i++)
        if (rgb_fourcc_list[i].vlc_fourcc == fmt->i_chroma
         && rgb_fourcc_list[i].red == fmt->i_rmask
         && rgb_fourcc_list[i].green == fmt->i_gmask
         && rgb_fourcc_list[i].blue == fmt->i_bmask)
            return rgb_fourcc_list[i].drm_fourcc;

    return DRM_FORMAT_INVALID;
}

vlc_fourcc_t vlc_fourcc_drm(uint_fast32_t drm_fourcc)
{
    for (size_t i = 0; i < ARRAY_SIZE(fourcc_list); i++)
        if (fourcc_list[i].drm_fourcc == drm_fourcc)
            return fourcc_list[i].vlc_fourcc;

    for (size_t i = 0; i < ARRAY_SIZE(rgb_fourcc_list); i++)
        if (rgb_fourcc_list[i].drm_fourcc == drm_fourcc)
            return rgb_fourcc_list[i].vlc_fourcc;

    return 0;
}

bool vlc_video_format_drm(video_format_t *restrict fmt,
                          uint_fast32_t drm_fourcc)
{
    for (size_t i = 0; i < ARRAY_SIZE(fourcc_list); i++)
        if (fourcc_list[i].drm_fourcc == drm_fourcc) {
            fmt->i_chroma = fourcc_list[i].vlc_fourcc;
            fmt->i_rmask = fmt->i_gmask = fmt->i_bmask = 0;
            return true;
        }

    for (size_t i = 0; i < ARRAY_SIZE(rgb_fourcc_list); i++)
        if (rgb_fourcc_list[i].drm_fourcc == drm_fourcc) {
            fmt->i_chroma = rgb_fourcc_list[i].vlc_fourcc;
            fmt->i_rmask = rgb_fourcc_list[i].red;
            fmt->i_gmask = rgb_fourcc_list[i].green;
            fmt->i_bmask = rgb_fourcc_list[i].blue;
            return true;
        }

    return false;
}
