#version 440

#define DITHERING

/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

// See `FadingEdgeMask` and `BackdropBlendItem`.

#extension GL_GOOGLE_include_directive : enable

#include "Common.glsl"

layout(location = 0) in vec2 coord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float vertical; // 1.0 for top and bottom edges
    float flip;     // 1.0 for bottom and right edges
};

void main()
{
    float pos = mix(coord.x, coord.y, vertical);
    pos = mix(pos, 1.0 - pos, flip);

    // 1.0 at the edge, 0.0 at the opposite edge:
    float coverage = 1.0 - smoothstep(0.0, 1.0, pos);

    fragColor = vec4(coverage * qt_Opacity);

#ifdef DITHERING
    float noise = (rand(coord) - 0.5) * DITHERING_STRENGTH;
    fragColor = clamp(fragColor + vec4(noise) * step(DITHERING_CUTOFF, fragColor.a), 0.0, 1.0);
#endif
}
