/*****************************************************************************
 * d3d_dynamic_shader.h: Direct3D Shader Blob generation
 *****************************************************************************
 * Copyright (C) 2017-2021 VLC authors and VideoLAN
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

#ifndef VLC_D3D_DYNAMIC_SHADER_H
#define VLC_D3D_DYNAMIC_SHADER_H

#include <d3dcommon.h>

#ifdef __cplusplus
extern "C" {
#endif// __cplusplus

HRESULT D3D_CompilePixelShader(vlc_object_t *,
                               D3D_FEATURE_LEVEL,
                               const display_info_t *,
                               video_transfer_func_t,
                               bool src_full_range,
                               const d3d_format_t *dxgi_fmt,
                               d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET],
                               size_t shader_views[DXGI_MAX_RENDER_TARGET]);

HRESULT D3D_CompileVertexShader(vlc_object_t *,
                                D3D_FEATURE_LEVEL, bool flat,
                                d3d_shader_blob *);

#ifdef __cplusplus
}
#endif// __cplusplus

#endif /* VLC_D3D_DYNAMIC_SHADER_H */
