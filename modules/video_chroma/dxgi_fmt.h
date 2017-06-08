/*****************************************************************************
 * dxgi_fmt.h : DXGI helper calls
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

#ifndef VLC_VIDEOCHROMA_DXGI_FMT_H_
#define VLC_VIDEOCHROMA_DXGI_FMT_H_

#include <dxgi.h>
#include <dxgiformat.h>

#include <vlc_common.h>
#include <vlc_fourcc.h>

#define D3D11_MAX_SHADER_VIEW  2

typedef struct
{
    const char   *name;
    DXGI_FORMAT  formatTexture;
    vlc_fourcc_t fourcc;
    uint8_t      bitsPerChannel;
    uint8_t      widthDenominator;
    uint8_t      heightDenominator;
    DXGI_FORMAT  resourceFormat[D3D11_MAX_SHADER_VIEW];
} d3d_format_t;

extern const char *DxgiFormatToStr(DXGI_FORMAT format);
extern vlc_fourcc_t DxgiFormatFourcc(DXGI_FORMAT format);
extern const d3d_format_t *GetRenderFormatList(void);
extern void DxgiFormatMask(DXGI_FORMAT format, video_format_t *);

typedef struct ID3D11Device ID3D11Device;
bool isXboxHardware(ID3D11Device *d3ddev);
bool isNvidiaHardware(ID3D11Device *d3ddev);
IDXGIAdapter *D3D11DeviceAdapter(ID3D11Device *d3ddev);

static inline bool DeviceSupportsFormat(ID3D11Device *d3ddevice,
                                        DXGI_FORMAT format, UINT supportFlags)
{
    UINT i_formatSupport;
    return SUCCEEDED( ID3D11Device_CheckFormatSupport(d3ddevice, format,
                                                      &i_formatSupport) )
            && ( i_formatSupport & supportFlags ) == supportFlags;
}

static inline const d3d_format_t *FindD3D11Format(ID3D11Device *d3ddevice,
                                                  vlc_fourcc_t i_src_chroma,
                                                  uint8_t bits_per_channel,
                                                  bool allow_opaque,
                                                  UINT supportFlags)
{
    supportFlags |= D3D11_FORMAT_SUPPORT_TEXTURE2D;
    for (const d3d_format_t *output_format = GetRenderFormatList();
         output_format->name != NULL; ++output_format)
    {
        if (i_src_chroma && i_src_chroma != output_format->fourcc)
            continue;
        if (bits_per_channel && bits_per_channel > output_format->bitsPerChannel)
            continue;
        if (!allow_opaque && (output_format->fourcc == VLC_CODEC_D3D11_OPAQUE ||
                              output_format->fourcc == VLC_CODEC_D3D11_OPAQUE_10B))
            continue;

        DXGI_FORMAT textureFormat;
        if (output_format->formatTexture == DXGI_FORMAT_UNKNOWN)
            textureFormat = output_format->resourceFormat[0];
        else
            textureFormat = output_format->formatTexture;

        if( DeviceSupportsFormat( d3ddevice, textureFormat, supportFlags ) )
            return output_format;
    }
    return NULL;
}

#endif /* include-guard */
