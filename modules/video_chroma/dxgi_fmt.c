/*****************************************************************************
 * d3d11_surface.c : D3D11 GPU surface conversion module for vlc
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

#include "dxgi_fmt.h"
#include <vlc_es.h>

typedef struct
{
    const char   *name;
    DXGI_FORMAT  format;
} dxgi_format_t;

static const dxgi_format_t dxgi_formats[] = {
    { "NV12",        DXGI_FORMAT_NV12                },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE          },
    { "RGBA",        DXGI_FORMAT_R8G8B8A8_UNORM      },
    { "RGBA_SRGB",   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
    { "BGRX",        DXGI_FORMAT_B8G8R8X8_UNORM      },
    { "BGRA",        DXGI_FORMAT_B8G8R8A8_UNORM      },
    { "BGRA_SRGB",   DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
    { "AYUV",        DXGI_FORMAT_AYUV                },
    { "YUY2",        DXGI_FORMAT_YUY2                },
    { "AI44",        DXGI_FORMAT_AI44                },
    { "P8",          DXGI_FORMAT_P8                  },
    { "A8P8",        DXGI_FORMAT_A8P8                },
    { "B5G6R5",      DXGI_FORMAT_B5G6R5_UNORM        },
    { "Y416",        DXGI_FORMAT_Y416                },
    { "P010",        DXGI_FORMAT_P010                },
    { "Y210",        DXGI_FORMAT_Y210                },
    { "Y410",        DXGI_FORMAT_Y410                },
    { "NV11",        DXGI_FORMAT_NV11                },
    { "UNKNOWN",     DXGI_FORMAT_UNKNOWN             },

    { NULL, 0,}
};

static const d3d_format_t d3d_formats[] = {
    { "NV12",     DXGI_FORMAT_NV12,           VLC_CODEC_NV12,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "VA_NV12",  DXGI_FORMAT_NV12,           VLC_CODEC_D3D11_OPAQUE, DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM },
#ifdef BROKEN_PIXEL
    { "YUY2",     DXGI_FORMAT_YUY2,           VLC_CODEC_I422,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "AYUV",     DXGI_FORMAT_AYUV,           VLC_CODEC_YUVA,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "Y416",     DXGI_FORMAT_Y416,           VLC_CODEC_I444_16L, DXGI_FORMAT_R16G16B16A16_UINT,  0 },
#endif
#ifdef UNTESTED
    { "P010",     DXGI_FORMAT_P010,           VLC_CODEC_P10,      DXGI_FORMAT_R16_UNORM,          DXGI_FORMAT_R16_UNORM },
    { "Y210",     DXGI_FORMAT_Y210,           VLC_CODEC_I422_10L, DXGI_FORMAT_R16G16B16A16_UNORM, 0 },
    { "Y410",     DXGI_FORMAT_Y410,           VLC_CODEC_I444_10L, DXGI_FORMAT_R10G10B10A2_UNORM,  0 },
    { "NV11",     DXGI_FORMAT_NV11,           VLC_CODEC_I411,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
#endif
    { "R8G8B8A8", DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_RGBA,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "VA_RGBA",  DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_D3D11_OPAQUE, DXGI_FORMAT_R8G8B8A8_UNORM, 0 },
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_BGRA,     DXGI_FORMAT_B8G8R8A8_UNORM,     0 },
    { "VA_BGRA",  DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_D3D11_OPAQUE, DXGI_FORMAT_B8G8R8A8_UNORM, 0 },
    { "R8G8B8X8", DXGI_FORMAT_B8G8R8X8_UNORM, VLC_CODEC_RGB32,    DXGI_FORMAT_B8G8R8X8_UNORM,     0 },
    { "B5G6R5",   DXGI_FORMAT_B5G6R5_UNORM,   VLC_CODEC_RGB16,    DXGI_FORMAT_B5G6R5_UNORM,       0 },

    { NULL, 0, 0, 0, 0}
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
