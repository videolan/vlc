/*****************************************************************************
 * d3d11_quad.h: Direct3D11 Quad handling
 *****************************************************************************
 * Copyright (C) 2017-2018 VLC authors and VideoLAN
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

#ifndef VLC_D3D11_QUAD_H
#define VLC_D3D11_QUAD_H

#include "../../video_chroma/d3d11_fmt.h"
#include "d3d11_shaders.h"
#ifdef HAVE_D3D11_4_H
# include <d3d11_4.h>
#endif

#define PS_CONST_LUMI_BOUNDS 0
#define VS_CONST_VIEWPOINT   1

typedef bool (*d3d11_select_plane_t)(void *opaque, size_t plane_index, ID3D11RenderTargetView **);

#ifdef HAVE_D3D11_4_H
struct d3d11_gpu_fence
{
    Microsoft::WRL::ComPtr<ID3D11Fence>          d3dRenderFence;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> d3dcontext4;
    UINT64                                       renderFence = 0;
    HANDLE                                       renderFinished = nullptr;
};

HRESULT D3D11_InitFence(d3d11_device_t &, d3d11_gpu_fence &);
int D3D11_WaitFence(d3d11_gpu_fence &);
void D3D11_ReleaseFence(d3d11_gpu_fence &);
#endif


void D3D11_RenderQuad(d3d11_device_t *, d3d11_quad_t *, d3d11_vertex_shader_t *,
                      ID3D11ShaderResourceView *resourceViews[DXGI_MAX_SHADER_VIEW],
                      d3d11_select_plane_t selectPlane, void *selectOpaque);

int D3D11_AllocateQuad(vlc_object_t *, d3d11_device_t *, video_projection_mode_t, d3d11_quad_t *);
#define D3D11_AllocateQuad(a,b,c,d)  D3D11_AllocateQuad(VLC_OBJECT(a),b,c,d)

int D3D11_SetupQuad(vlc_object_t *, d3d11_device_t *, const video_format_t *, d3d11_quad_t *,
                    const display_info_t *);
#define D3D11_SetupQuad(a,b,c,d,e)  D3D11_SetupQuad(VLC_OBJECT(a),b,c,d,e)

bool D3D11_UpdateQuadPosition( vlc_object_t *, d3d11_device_t *, d3d11_quad_t *,
                               const RECT *output, video_transform_t );
#define D3D11_UpdateQuadPosition(a,b,c,d,e)  D3D11_UpdateQuadPosition(VLC_OBJECT(a),b,c,d,e)

void D3D11_UpdateQuadOpacity(vlc_object_t *, d3d11_device_t *, d3d11_quad_t *, float opacity);
#define D3D11_UpdateQuadOpacity(a,b,c,d)  D3D11_UpdateQuadOpacity(VLC_OBJECT(a),b,c,d)

void D3D11_UpdateQuadLuminanceScale(vlc_object_t *, d3d11_device_t *, d3d11_quad_t *, float luminanceScale);
#define D3D11_UpdateQuadLuminanceScale(a,b,c,d)  D3D11_UpdateQuadLuminanceScale(VLC_OBJECT(a),b,c,d)

void D3D11_UpdateViewpoint(vlc_object_t *, d3d11_device_t *, d3d11_quad_t *, const vlc_viewpoint_t*, float sar);
#define D3D11_UpdateViewpoint(a,b,c,d,e)  D3D11_UpdateViewpoint(VLC_OBJECT(a),b,c,d,e)

#endif /* VLC_D3D11_QUAD_H */
