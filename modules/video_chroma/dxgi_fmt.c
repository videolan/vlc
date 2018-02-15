/*****************************************************************************
 * dxgi_fmt.c : DXGI GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright © 2015 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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
# include "config.h"
#endif

#include <vlc_es.h>

#include "dxgi_fmt.h"

typedef struct
{
    const char   *name;
    DXGI_FORMAT  format;
    vlc_fourcc_t vlc_format;
} dxgi_format_t;

static const dxgi_format_t dxgi_formats[] = {
    { "NV12",        DXGI_FORMAT_NV12,                VLC_CODEC_NV12     },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE,          0                  },
    { "RGBA",        DXGI_FORMAT_R8G8B8A8_UNORM,      VLC_CODEC_RGBA     },
    { "RGBA_SRGB",   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, VLC_CODEC_RGBA     },
    { "BGRX",        DXGI_FORMAT_B8G8R8X8_UNORM,      VLC_CODEC_RGB32    },
    { "BGRA",        DXGI_FORMAT_B8G8R8A8_UNORM,      VLC_CODEC_BGRA     },
    { "BGRA_SRGB",   DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, VLC_CODEC_BGRA     },
    { "AYUV",        DXGI_FORMAT_AYUV,                VLC_CODEC_YUVA     },
    { "YUY2",        DXGI_FORMAT_YUY2,                VLC_CODEC_YUYV     },
    { "AI44",        DXGI_FORMAT_AI44,                0                  },
    { "P8",          DXGI_FORMAT_P8,                  0                  },
    { "A8P8",        DXGI_FORMAT_A8P8,                0                  },
    { "B5G6R5",      DXGI_FORMAT_B5G6R5_UNORM,        VLC_CODEC_RGB16    },
    { "Y416",        DXGI_FORMAT_Y416,                0                  },
    { "P010",        DXGI_FORMAT_P010,                VLC_CODEC_P010     },
    { "P016",        DXGI_FORMAT_P016,                0                  },
    { "Y210",        DXGI_FORMAT_Y210,                VLC_CODEC_YUYV     }, /* AV_PIX_FMT_YUYV422 */
    { "Y410",        DXGI_FORMAT_Y410,                0                  },
    { "NV11",        DXGI_FORMAT_NV11,                0                  },
    { "RGB10A2",     DXGI_FORMAT_R10G10B10A2_UNORM,   0                  },
    { "RGB16",       DXGI_FORMAT_R16G16B16A16_UNORM,  0                  },
    { "RGB16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT,  0                  },
    { "UNKNOWN",     DXGI_FORMAT_UNKNOWN,             0                  },

    { NULL, 0, 0}
};

static const d3d_format_t d3d_formats[] = {
    { "NV12",     DXGI_FORMAT_NV12,           VLC_CODEC_NV12,              8, 2, 2, { DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM } },
    { "VA_NV12",  DXGI_FORMAT_NV12,           VLC_CODEC_D3D11_OPAQUE,      8, 2, 2, { DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM } },
    { "P010",     DXGI_FORMAT_P010,           VLC_CODEC_P010,             10, 2, 2, { DXGI_FORMAT_R16_UNORM,      DXGI_FORMAT_R16G16_UNORM } },
    { "VA_P010",  DXGI_FORMAT_P010,           VLC_CODEC_D3D11_OPAQUE_10B, 10, 2, 2, { DXGI_FORMAT_R16_UNORM,      DXGI_FORMAT_R16G16_UNORM } },
    { "YUY2",     DXGI_FORMAT_YUY2,           VLC_CODEC_YUYV,              8, 2, 2, { DXGI_FORMAT_R8G8B8A8_UNORM } },
#ifdef BROKEN_PIXEL
    { "AYUV",     DXGI_FORMAT_AYUV,           VLC_CODEC_YUVA,          8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "Y416",     DXGI_FORMAT_Y416,           VLC_CODEC_I444_16L,     16, 1, 1, { DXGI_FORMAT_R16G16B16A16_UINT } },
#endif
#ifdef UNTESTED
    { "Y210",     DXGI_FORMAT_Y210,           VLC_CODEC_I422_10L,     10, 2, 1, { DXGI_FORMAT_R16G16B16A16_UNORM } },
    { "Y410",     DXGI_FORMAT_Y410,           VLC_CODEC_I444,         10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "NV11",     DXGI_FORMAT_NV11,           VLC_CODEC_I411,          8, 4, 1, { DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM} },
#endif
    { "I420",     DXGI_FORMAT_UNKNOWN,        VLC_CODEC_I420,          8, 2, 2, { DXGI_FORMAT_R8_UNORM,      DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM } },
    { "I420_10",  DXGI_FORMAT_UNKNOWN,        VLC_CODEC_I420_10L,     10, 2, 2, { DXGI_FORMAT_R16_UNORM,     DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM } },
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_BGRA,          8, 1, 1, { DXGI_FORMAT_B8G8R8A8_UNORM } },
    { "VA_BGRA",  DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_D3D11_OPAQUE,  8, 1, 1, { DXGI_FORMAT_B8G8R8A8_UNORM } },
    { "R8G8B8A8", DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_RGBA,          8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "VA_RGBA",  DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_D3D11_OPAQUE,  8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "R8G8B8X8", DXGI_FORMAT_B8G8R8X8_UNORM, VLC_CODEC_RGB32,         8, 1, 1, { DXGI_FORMAT_B8G8R8X8_UNORM } },
    { "B5G6R5",   DXGI_FORMAT_B5G6R5_UNORM,   VLC_CODEC_RGB16,         5, 1, 1, { DXGI_FORMAT_B5G6R5_UNORM } },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE,  VLC_CODEC_D3D11_OPAQUE,  8, 2, 2, { DXGI_FORMAT_UNKNOWN } },

    { NULL, 0, 0, 0, 0, 0, {} }
};

const char *DxgiFormatToStr(DXGI_FORMAT format)
{
    for (const dxgi_format_t *f = dxgi_formats; f->name != NULL; ++f)
    {
        if (f->format == format)
            return f->name;
    }
    return NULL;
}

vlc_fourcc_t DxgiFormatFourcc(DXGI_FORMAT format)
{
    for (const dxgi_format_t *f = dxgi_formats; f->name != NULL; ++f)
    {
        if (f->format == format)
            return f->vlc_format;
    }
    return 0;
}

const d3d_format_t *GetRenderFormatList(void)
{
    return d3d_formats;
}

void DxgiFormatMask(DXGI_FORMAT format, video_format_t *fmt)
{
    if (format == DXGI_FORMAT_B8G8R8X8_UNORM)
    {
        fmt->i_rmask = 0x0000ff00;
        fmt->i_gmask = 0x00ff0000;
        fmt->i_bmask = 0xff000000;
    }
}

const char *DxgiVendorStr(int gpu_vendor)
{
    static const struct {
        unsigned   id;
        const char name[32];
    } vendors [] = {
        { GPU_MANUFACTURER_AMD,      "ATI"         },
        { GPU_MANUFACTURER_NVIDIA,   "NVIDIA"      },
        { GPU_MANUFACTURER_VIA,      "VIA"         },
        { GPU_MANUFACTURER_INTEL,    "Intel"       },
        { GPU_MANUFACTURER_S3,       "S3 Graphics" },
        { GPU_MANUFACTURER_QUALCOMM, "Qualcomm"    },
        { 0,                         "Unknown" }
    };

    int i = 0;
    for (i = 0; vendors[i].id != 0; i++) {
        if (vendors[i].id == gpu_vendor)
            break;
    }
    return vendors[i].name;
}

UINT DxgiResourceCount(const d3d_format_t *d3d_fmt)
{
    for (UINT count=0; count<D3D11_MAX_SHADER_VIEW; count++)
    {
        if (d3d_fmt->resourceFormat[count] == DXGI_FORMAT_UNKNOWN)
            return count;
    }
    return D3D11_MAX_SHADER_VIEW;
}
