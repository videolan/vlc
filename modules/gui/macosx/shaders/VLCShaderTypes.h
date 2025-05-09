/*****************************************************************************
 * VLCShaderTypes.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLCShaderTypes_h
#define VLCShaderTypes_h

#include <simd/simd.h>

// These structs are shared between C/Objective-C and Metal.

typedef struct {
    vector_float3 initialPosition; // x, y (initial screen position), z (depth/parallax factor 0.0-1.0)
    float randomSeed;              // (0.0-1.0)
} Snowflake;

typedef struct {
    float time;                    // Current animation time
    vector_float2 resolution;      // Viewport width and height
} Uniforms;

typedef struct {
    vector_float2 position;        // Local offset for the quad vertex (e.g., -0.5 to 0.5)
} VertexIn;


#ifdef __METAL_VERSION__

typedef struct {
    float4 position       [[position]]; // Clip-space position (required output for vertex shader)
    float2 texCoord;                    // Texture coordinate (0-1) across the snowflake quad
} VertexOut;

#endif // __METAL_VERSION__

#endif
