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

#include "../../video_chroma/dxgi_fmt.h"

#include <vlc_es.h>

#define DEFAULT_BRIGHTNESS         100
#define DEFAULT_SRGB_BRIGHTNESS    100
#define MAX_HLG_BRIGHTNESS        1000
#define MAX_PQ_BRIGHTNESS        10000

typedef struct {
    video_color_primaries_t  primaries;
    video_transfer_func_t    transfer;
    video_color_space_t      color;
    bool                     b_full_range;
    unsigned                 luminance_peak;
    const d3d_format_t       *pixelFormat;
} display_info_t;

/* structures passed to the pixel shader */
typedef struct {
    FLOAT Colorspace[4*4];
    FLOAT Primaries[4*4];
    FLOAT Opacity;
    FLOAT LuminanceScale;
    FLOAT BoundaryX;
    FLOAT BoundaryY;
    FLOAT padding[28]; // 256 bytes alignment
} PS_CONSTANT_BUFFER;

typedef struct {
    FLOAT View[4*4];
    FLOAT Zoom[4*4];
    FLOAT Projection[4*4];
    FLOAT padding[16]; // 256 bytes alignment
} VS_PROJECTION_CONST;

typedef struct {
    struct {
        FLOAT x;
        FLOAT y;
        FLOAT z;
    } position;
    struct {
        FLOAT uv[2];
    } texture;
} d3d_vertex_t;


/* A Quad is texture that can be displayed in a rectangle */
typedef struct
{
    const d3d_format_t        *textureFormat;
    UINT                      vertexCount;
    UINT                      vertexStride;
    UINT                      indexCount;
    video_projection_mode_t   projection;

    unsigned int              i_width;
    unsigned int              i_height;

    PS_CONSTANT_BUFFER        *shaderConstants;
    VS_PROJECTION_CONST       *vertexConstants;

} d3d_quad_t;

typedef struct d3d_shader_blob
{
    void *opaque;
    void (*pf_release)(struct d3d_shader_blob *);
    SIZE_T buf_size;
    void *buffer;
} d3d_shader_blob;

static inline void D3D_ShaderBlobRelease(d3d_shader_blob *blob)
{
    if (blob->pf_release)
        blob->pf_release(blob);
    *blob = (d3d_shader_blob) { 0 };
}

float D3D_GetFormatLuminance(vlc_object_t *, const video_format_t *);
#define D3D_GetFormatLuminance(a,b)  D3D_GetFormatLuminance(VLC_OBJECT(a),b)

bool D3D_UpdateQuadOpacity(d3d_quad_t *, float opacity);
bool D3D_UpdateQuadLuminanceScale(d3d_quad_t *, float luminanceScale);

void D3D_SetupQuad(vlc_object_t *, const video_format_t *, d3d_quad_t *,
                   const display_info_t *);

bool D3D_QuadSetupBuffers(vlc_object_t *, d3d_quad_t *, video_projection_mode_t);
bool D3D_SetupQuadData(vlc_object_t *, d3d_quad_t *, const RECT *, d3d_vertex_t*, void *, video_orientation_t);

void D3D_UpdateViewpoint(d3d_quad_t *, const vlc_viewpoint_t *, float f_sar);

#endif /* VLC_D3D_SHADERS_H */
