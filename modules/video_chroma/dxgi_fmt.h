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

#define GPU_MANUFACTURER_AMD           0x1002
#define GPU_MANUFACTURER_NVIDIA        0x10DE
#define GPU_MANUFACTURER_VIA           0x1106
#define GPU_MANUFACTURER_INTEL         0x8086
#define GPU_MANUFACTURER_S3            0x5333
#define GPU_MANUFACTURER_QUALCOMM  0x4D4F4351

#define D3D11_MAX_SHADER_VIEW  3

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

const char *DxgiFormatToStr(DXGI_FORMAT format);
vlc_fourcc_t DxgiFormatFourcc(DXGI_FORMAT format);
const d3d_format_t *GetRenderFormatList(void);
void DxgiFormatMask(DXGI_FORMAT format, video_format_t *);
const char *DxgiVendorStr(int gpu_vendor);
UINT DxgiResourceCount(const d3d_format_t *);

#endif /* include-guard */
