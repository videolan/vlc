/*****************************************************************************
 * VLCSnowShader.metal: MacOS X interface module
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

#include <metal_stdlib>
using namespace metal;

#include "VLCShaderTypes.h"

// It calculates the final screen position for that vertex of that snowflake.
vertex VertexOut snowVertexShader(
    uint vid [[vertex_id]],     // Index of the current vertex in the input buffer (0-5 for a quad)
    uint iid [[instance_id]],   // Index of the current instance (snowflake)
    constant VertexIn *vertices [[buffer(0)]],             // Per-vertex data (quad corners)
    constant Snowflake *snowflakes [[buffer(1)]],          // Per-instance snowflake data
    constant Uniforms &uniforms [[buffer(2)]]              // Global uniforms
) {
    VertexOut out;

    Snowflake currentSnowflake = snowflakes[iid];
    const float fallSpeed = 0.15 + currentSnowflake.randomSeed * 0.1;
    // Closer flakes (larger z) fall faster & are bigger
    const float parallaxFactor = 0.5 + currentSnowflake.initialPosition.z * 1.5;

    // Calculate current Y position (falling down, wrapping around)
    float currentYNormalized = currentSnowflake.initialPosition.y - (uniforms.time * fallSpeed * parallaxFactor);
    currentYNormalized = fmod(currentYNormalized, 2.0f); // Wrap around a range of 2.0 (e.g. +1 to -1)
    if (currentYNormalized < -1.0f) { // Ensure it wraps from bottom to top correctly
        currentYNormalized += 2.0f;
    }

    // Horizontal sway using sine wave based on time and random seed
    const float swayAmount = 0.05 + currentSnowflake.randomSeed * 0.05;
    const float swayFrequency = 1.0 + currentSnowflake.randomSeed * 0.5;
    const float currentXNormalized = currentSnowflake.initialPosition.x + sin(uniforms.time * swayFrequency + currentSnowflake.randomSeed * 10.0) * swayAmount;

    // Make closer flakes appear larger
    const float flakeBaseSize = 0.02; // Base size in NDC
    const float flakeScreenSize = flakeBaseSize * parallaxFactor;

    const vector_float2 quadVertexOffset = vertices[vid].position; // e.g., from -0.5 to 0.5

    const float aspectRatio = uniforms.resolution.x / uniforms.resolution.y;
    vector_float2 finalPositionNDC;
    finalPositionNDC.x = currentXNormalized + (quadVertexOffset.x * flakeScreenSize);
    finalPositionNDC.y = currentYNormalized + (quadVertexOffset.y * flakeScreenSize * aspectRatio) ; // Apply aspect ratio to y scaling of quad

    out.position = float4(finalPositionNDC.x, finalPositionNDC.y, currentSnowflake.initialPosition.z, 1.0);
    out.texCoord = vertices[vid].position + 0.5; // Convert -0.5..0.5 to 0..1
    return out;
}

// Takes data from the vertex shader (VertexOut) for the current pixel
// and determines its final color.
fragment float4 snowFragmentShader(
    VertexOut in [[stage_in]],
    constant Uniforms &uniforms [[buffer(0)]] // Uniforms can also be accessed here if needed
) {
    const float dist = distance(in.texCoord, float2(0.5));
    // Create a smooth alpha falloff for a soft circle
    const float alpha = 0.75 - smoothstep(0.0, 0.5, dist);
    return float4(1.0, 1.0, 1.0, alpha);
}
