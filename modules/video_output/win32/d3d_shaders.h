/*****************************************************************************
 * d3d_shaders.h: Direct3D Shaders
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

#ifndef VLC_D3D_SHADERS_H
#define VLC_D3D_SHADERS_H

#include <d3dcompiler.h> // for pD3DCompile

typedef struct
{
    HINSTANCE                 compiler_dll; /* handle of the opened d3dcompiler dll */
    pD3DCompile               OurD3DCompile;
} d3d_shader_compiler_t;

int D3D_InitShaders(vlc_object_t *, d3d_shader_compiler_t *);
void D3D_ReleaseShaders(d3d_shader_compiler_t *);

#endif /* VLC_D3D_SHADERS_H */
