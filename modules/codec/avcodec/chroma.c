/*****************************************************************************
 * chroma.c: libavutil <-> libvlc conversion routines
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 021100301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_codec.h>

#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include "avcommon.h"
#include "chroma.h"

/*****************************************************************************
 * Chroma fourcc -> libavutil pixfmt mapping
 *****************************************************************************/
#if defined(WORDS_BIGENDIAN)
#   define VLC_RGB_ES( fcc, leid, beid ) \
    { fcc, beid, 0, 0, 0 },
#else
#   define VLC_RGB_ES( fcc, leid, beid ) \
    { fcc, leid, 0, 0, 0 },
#endif

#define VLC_RGB( fcc, leid, beid, rmask, gmask, bmask ) \
    { fcc, leid, rmask, gmask, bmask }, \
    { fcc, beid, bmask, gmask, rmask }, \
    VLC_RGB_ES( fcc, leid, beid )


static const struct
{
    vlc_fourcc_t  i_chroma;
    int           i_chroma_id;
    uint32_t      i_rmask;
    uint32_t      i_gmask;
    uint32_t      i_bmask;

} chroma_table[] =
{
    /* Planar YUV formats */
    {VLC_CODEC_I444, AV_PIX_FMT_YUV444P, 0, 0, 0 },
    {VLC_CODEC_J444, AV_PIX_FMT_YUVJ444P, 0, 0, 0 },

    {VLC_CODEC_I440, AV_PIX_FMT_YUV440P, 0, 0, 0 },
    {VLC_CODEC_J440, AV_PIX_FMT_YUVJ440P, 0, 0, 0 },

    {VLC_CODEC_I422, AV_PIX_FMT_YUV422P, 0, 0, 0 },
    {VLC_CODEC_J422, AV_PIX_FMT_YUVJ422P, 0, 0, 0 },

    {VLC_CODEC_I420, AV_PIX_FMT_YUV420P, 0, 0, 0 },
    {VLC_CODEC_YV12, AV_PIX_FMT_YUV420P, 0, 0, 0 },
    {VLC_FOURCC('I','Y','U','V'), AV_PIX_FMT_YUV420P, 0, 0, 0 },
    {VLC_CODEC_J420, AV_PIX_FMT_YUVJ420P, 0, 0, 0 },
    {VLC_CODEC_I411, AV_PIX_FMT_YUV411P, 0, 0, 0 },
    {VLC_CODEC_I410, AV_PIX_FMT_YUV410P, 0, 0, 0 },
    {VLC_FOURCC('Y','V','U','9'), AV_PIX_FMT_YUV410P, 0, 0, 0 },

    {VLC_CODEC_NV12, AV_PIX_FMT_NV12, 0, 0, 0 },
    {VLC_CODEC_NV21, AV_PIX_FMT_NV21, 0, 0, 0 },

    {VLC_CODEC_I420_9L, AV_PIX_FMT_YUV420P9LE, 0, 0, 0 },
    {VLC_CODEC_I420_9B, AV_PIX_FMT_YUV420P9BE, 0, 0, 0 },
    {VLC_CODEC_I420_10L, AV_PIX_FMT_YUV420P10LE, 0, 0, 0 },
    {VLC_CODEC_I420_10B, AV_PIX_FMT_YUV420P10BE, 0, 0, 0 },
#ifdef AV_PIX_FMT_YUV420P12 /* 54, 17, 100 */
    {VLC_CODEC_I420_12L, AV_PIX_FMT_YUV420P12LE, 0, 0, 0 },
    {VLC_CODEC_I420_12B, AV_PIX_FMT_YUV420P12BE, 0, 0, 0 },
#endif
    {VLC_CODEC_I420_16L, AV_PIX_FMT_YUV420P16LE, 0, 0, 0 },
    {VLC_CODEC_I420_16B, AV_PIX_FMT_YUV420P16BE, 0, 0, 0 },
#ifdef AV_PIX_FMT_P010
    {VLC_CODEC_P010, AV_PIX_FMT_P010, 0, 0, 0 },
#endif
#ifdef AV_PIX_FMT_P016
    {VLC_CODEC_P016, AV_PIX_FMT_P016, 0, 0, 0 },
#endif

    {VLC_CODEC_I422_9L, AV_PIX_FMT_YUV422P9LE, 0, 0, 0 },
    {VLC_CODEC_I422_9B, AV_PIX_FMT_YUV422P9BE, 0, 0, 0 },
    {VLC_CODEC_I422_10L, AV_PIX_FMT_YUV422P10LE, 0, 0, 0 },
    {VLC_CODEC_I422_10B, AV_PIX_FMT_YUV422P10BE, 0, 0, 0 },
    {VLC_CODEC_I422_16L, AV_PIX_FMT_YUV422P16LE, 0, 0, 0 },
    {VLC_CODEC_I422_16B, AV_PIX_FMT_YUV422P16BE, 0, 0, 0 },
#ifdef AV_PIX_FMT_YUV422P12 /* 54, 17, 100 */
    {VLC_CODEC_I422_12L, AV_PIX_FMT_YUV422P12LE, 0, 0, 0 },
    {VLC_CODEC_I422_12B, AV_PIX_FMT_YUV422P12BE, 0, 0, 0 },
#endif

    {VLC_CODEC_YUV420A, AV_PIX_FMT_YUVA420P, 0, 0, 0 },
    {VLC_CODEC_YUV422A, AV_PIX_FMT_YUVA422P, 0, 0, 0 },
    {VLC_CODEC_YUVA,    AV_PIX_FMT_YUVA444P, 0, 0, 0 },

    {VLC_CODEC_YUVA_444_10L, AV_PIX_FMT_YUVA444P10LE, 0, 0, 0 },
    {VLC_CODEC_YUVA_444_10B, AV_PIX_FMT_YUVA444P10BE, 0, 0, 0 },

    {VLC_CODEC_I444_9L, AV_PIX_FMT_YUV444P9LE, 0, 0, 0 },
    {VLC_CODEC_I444_9B, AV_PIX_FMT_YUV444P9BE, 0, 0, 0 },
    {VLC_CODEC_I444_10L, AV_PIX_FMT_YUV444P10LE, 0, 0, 0 },
    {VLC_CODEC_I444_10B, AV_PIX_FMT_YUV444P10BE, 0, 0, 0 },
#ifdef AV_PIX_FMT_YUV444P12 /* 54, 17, 100 */
    {VLC_CODEC_I444_12L, AV_PIX_FMT_YUV444P12LE, 0, 0, 0 },
    {VLC_CODEC_I444_12B, AV_PIX_FMT_YUV444P12BE, 0, 0, 0 },
#endif
    {VLC_CODEC_I444_16L, AV_PIX_FMT_YUV444P16LE, 0, 0, 0 },
    {VLC_CODEC_I444_16B, AV_PIX_FMT_YUV444P16BE, 0, 0, 0 },

    /* Packed YUV formats */
    {VLC_CODEC_YUYV, AV_PIX_FMT_YUYV422, 0, 0, 0 },
    {VLC_FOURCC('Y','U','Y','V'), AV_PIX_FMT_YUYV422, 0, 0, 0 },
    {VLC_CODEC_UYVY, AV_PIX_FMT_UYVY422, 0, 0, 0 },
    {VLC_CODEC_YVYU, AV_PIX_FMT_YVYU422, 0, 0, 0 },
    {VLC_FOURCC('Y','4','1','1'), AV_PIX_FMT_UYYVYY411, 0, 0, 0 },

    /* Packed RGB formats */
    VLC_RGB( VLC_FOURCC('R','G','B','4'), AV_PIX_FMT_RGB4, AV_PIX_FMT_BGR4, 0x10, 0x06, 0x01 )
    VLC_RGB( VLC_CODEC_RGB8, AV_PIX_FMT_RGB8, AV_PIX_FMT_BGR8, 0xC0, 0x38, 0x07 )

    VLC_RGB( VLC_CODEC_RGB15, AV_PIX_FMT_RGB555, AV_PIX_FMT_BGR555, 0x7c00, 0x03e0, 0x001f )
    VLC_RGB( VLC_CODEC_RGB16, AV_PIX_FMT_RGB565, AV_PIX_FMT_BGR565, 0xf800, 0x07e0, 0x001f )
    VLC_RGB( VLC_CODEC_RGB24, AV_PIX_FMT_RGB24, AV_PIX_FMT_BGR24, 0xff0000, 0x00ff00, 0x0000ff )

    VLC_RGB( VLC_CODEC_RGB32, AV_PIX_FMT_RGB32, AV_PIX_FMT_BGR32, 0x00ff0000, 0x0000ff00, 0x000000ff )
    VLC_RGB( VLC_CODEC_RGB32, AV_PIX_FMT_RGB32_1, AV_PIX_FMT_BGR32_1, 0xff000000, 0x00ff0000, 0x0000ff00 )

#ifdef AV_PIX_FMT_0BGR32
    VLC_RGB( VLC_CODEC_RGB32, AV_PIX_FMT_0BGR32, AV_PIX_FMT_0RGB32, 0x000000ff, 0x0000ff00, 0x00ff0000 )
#endif

#ifdef WORDS_BIGENDIAN
    {VLC_CODEC_RGBA64, AV_PIX_FMT_RGBA64BE, 0, 0, 0 },
#else /* !WORDS_BIGENDIAN */
    {VLC_CODEC_RGBA64, AV_PIX_FMT_RGBA64LE, 0, 0, 0 },
#endif /* !WORDS_BIGENDIAN */

    {VLC_CODEC_RGBA, AV_PIX_FMT_RGBA, 0, 0, 0 },
    {VLC_CODEC_ARGB, AV_PIX_FMT_ARGB, 0, 0, 0 },
    {VLC_CODEC_BGRA, AV_PIX_FMT_BGRA, 0, 0, 0 },
    {VLC_CODEC_GREY, AV_PIX_FMT_GRAY8, 0, 0, 0},
#ifdef AV_PIX_FMT_GRAY10
    {VLC_CODEC_GREY_10L, AV_PIX_FMT_GRAY10LE, 0, 0, 0},
    {VLC_CODEC_GREY_10B, AV_PIX_FMT_GRAY10BE, 0, 0, 0},
#endif
#ifdef AV_PIX_FMT_GRAY12
    {VLC_CODEC_GREY_12L, AV_PIX_FMT_GRAY12LE, 0, 0, 0},
    {VLC_CODEC_GREY_12B, AV_PIX_FMT_GRAY12BE, 0, 0, 0},
#endif
    {VLC_CODEC_GREY_16L, AV_PIX_FMT_GRAY16LE, 0, 0, 0},
    {VLC_CODEC_GREY_16B, AV_PIX_FMT_GRAY16BE, 0, 0, 0},

     /* Paletized RGB */
    {VLC_CODEC_RGBP, AV_PIX_FMT_PAL8, 0, 0, 0},

    {VLC_CODEC_GBR_PLANAR, AV_PIX_FMT_GBRP, 0, 0, 0 },
    {VLC_CODEC_GBR_PLANAR_9L, AV_PIX_FMT_GBRP9LE, 0, 0, 0 },
    {VLC_CODEC_GBR_PLANAR_9B, AV_PIX_FMT_GBRP9BE, 0, 0, 0 },
    {VLC_CODEC_GBR_PLANAR_10L, AV_PIX_FMT_GBRP10LE, 0, 0, 0 },
    {VLC_CODEC_GBR_PLANAR_10B, AV_PIX_FMT_GBRP10BE, 0, 0, 0 },
#ifdef AV_PIX_FMT_GBRP12 /* 55, 24, 0 / 51, 74, 100 */
    {VLC_CODEC_GBR_PLANAR_12L, AV_PIX_FMT_GBRP12LE, 0, 0, 0 },
    {VLC_CODEC_GBR_PLANAR_12B, AV_PIX_FMT_GBRP12BE, 0, 0, 0 },
#endif
#ifdef AV_PIX_FMT_GBRP14 /* ffmpeg only */
    {VLC_CODEC_GBR_PLANAR_14L, AV_PIX_FMT_GBRP14LE, 0, 0, 0 },
    {VLC_CODEC_GBR_PLANAR_14B, AV_PIX_FMT_GBRP14BE, 0, 0, 0 },
#endif
    {VLC_CODEC_GBR_PLANAR_16L, AV_PIX_FMT_GBRP16LE, 0, 0, 0 },
    {VLC_CODEC_GBR_PLANAR_16B, AV_PIX_FMT_GBRP16BE, 0, 0, 0 },
    {VLC_CODEC_GBRA_PLANAR, AV_PIX_FMT_GBRAP, 0, 0, 0 },
#ifdef AV_PIX_FMT_GBRAP10 /* 56, 1, 0 / 55, 25, 100 */
    {VLC_CODEC_GBRA_PLANAR_10L, AV_PIX_FMT_GBRAP10LE, 0, 0, 0 },
    {VLC_CODEC_GBRA_PLANAR_10B, AV_PIX_FMT_GBRAP10BE, 0, 0, 0 },
#endif
#ifdef AV_PIX_FMT_GBRAP12 /* 55, 25, 0, 19, 100 */
    {VLC_CODEC_GBRA_PLANAR_12L, AV_PIX_FMT_GBRAP12LE, 0, 0, 0 },
    {VLC_CODEC_GBRA_PLANAR_12B, AV_PIX_FMT_GBRAP12BE, 0, 0, 0 },
#endif
    {VLC_CODEC_GBRA_PLANAR_16L, AV_PIX_FMT_GBRAP16LE, 0, 0, 0 },
    {VLC_CODEC_GBRA_PLANAR_16B, AV_PIX_FMT_GBRAP16BE, 0, 0, 0 },

    /* XYZ */
    {VLC_CODEC_XYZ12, AV_PIX_FMT_XYZ12, 0xfff0, 0xfff0, 0xfff0},
    { 0, 0, 0, 0, 0 }
};

/* FIXME special case the RGB formats */
int GetFfmpegChroma( int *restrict i_ffmpeg_chroma, const video_format_t *fmt )
{
    for( int i = 0; chroma_table[i].i_chroma != 0; i++ )
    {
        if( chroma_table[i].i_chroma == fmt->i_chroma )
        {
            if( ( chroma_table[i].i_rmask == 0 &&
                  chroma_table[i].i_gmask == 0 &&
                  chroma_table[i].i_bmask == 0 ) ||
                ( chroma_table[i].i_rmask == fmt->i_rmask &&
                  chroma_table[i].i_gmask == fmt->i_gmask &&
                  chroma_table[i].i_bmask == fmt->i_bmask ) )
            {
                *i_ffmpeg_chroma = chroma_table[i].i_chroma_id;
                return VLC_SUCCESS;
            }
        }
    }
    return VLC_EGENERIC;
}

vlc_fourcc_t FindVlcChroma( int i_ffmpeg_id )
{
    for( int i = 0; chroma_table[i].i_chroma != 0; i++ )
        if( chroma_table[i].i_chroma_id == i_ffmpeg_id )
            return chroma_table[i].i_chroma;
    return 0;
}

int GetVlcChroma( video_format_t *fmt, int i_ffmpeg_chroma )
{
    for( int i = 0; chroma_table[i].i_chroma != 0; i++ )
    {
        if( chroma_table[i].i_chroma_id == i_ffmpeg_chroma )
        {
            fmt->i_rmask = chroma_table[i].i_rmask;
            fmt->i_gmask = chroma_table[i].i_gmask;
            fmt->i_bmask = chroma_table[i].i_bmask;
            fmt->i_chroma = chroma_table[i].i_chroma;
            video_format_FixRgb( fmt );
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

int FindFfmpegChroma( vlc_fourcc_t fourcc )
{
    for( int i = 0; chroma_table[i].i_chroma != 0; i++ )
        if( chroma_table[i].i_chroma == fourcc )
            return chroma_table[i].i_chroma_id;
    return AV_PIX_FMT_NONE;
}
