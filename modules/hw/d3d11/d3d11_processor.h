/*****************************************************************************
 * d3d11_processor.h: D3D11 VideoProcessor helper
 *****************************************************************************
 * Copyright © 2017 VLC authors, VideoLAN and VideoLabs
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

#ifndef VLC_D3D11_PROCESSOR_H
#define VLC_D3D11_PROCESSOR_H

#include <vlc_common.h>

#include "../../video_chroma/d3d11_fmt.h"

#ifdef ID3D11VideoContext_VideoProcessorBlt
typedef struct
{
    ID3D11VideoDevice              *d3dviddev;
    ID3D11VideoContext             *d3dvidctx;
    ID3D11VideoProcessorEnumerator *procEnumerator;
    ID3D11VideoProcessor           *videoProcessor;
} d3d11_processor_t;

int D3D11_CreateProcessor(vlc_object_t *, d3d11_device_t *,
                          D3D11_VIDEO_FRAME_FORMAT srcFields,
                          const video_format_t *fmt_in, const video_format_t *fmt_out,
                          d3d11_processor_t *out);
#define D3D11_CreateProcessor(a,b,c,d,e,f) D3D11_CreateProcessor(VLC_OBJECT(a),b,c,d,e,f)

void D3D11_ReleaseProcessor(d3d11_processor_t *);

HRESULT D3D11_Assert_ProcessorInput(vlc_object_t *, d3d11_processor_t *, picture_sys_d3d11_t *);
#define D3D11_Assert_ProcessorInput(a,b,c) D3D11_Assert_ProcessorInput(VLC_OBJECT(a),b,c)
#endif

#endif /* VLC_D3D11_PROCESSOR_H */
