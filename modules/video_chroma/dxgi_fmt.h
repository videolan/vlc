/*****************************************************************************
 * d3d11_surface.c : DXGI helper calls
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

#ifndef _VLC_VIDEOCHROMA_DXGI_H
#define _VLC_VIDEOCHROMA_DXGI_H 1

#include <dxgiformat.h>

#include <vlc_common.h>
#include <vlc_fourcc.h>

typedef struct
{
    const char   *name;
    DXGI_FORMAT  formatTexture;
    vlc_fourcc_t fourcc;
    DXGI_FORMAT  formatY;
    DXGI_FORMAT  formatUV;
} d3d_format_t;

extern const char *DxgiFormatToStr(DXGI_FORMAT format);
extern const d3d_format_t *GetRenderFormatList(void);

#endif /* _VLC_VIDEOCHROMA_DXGI_H */
