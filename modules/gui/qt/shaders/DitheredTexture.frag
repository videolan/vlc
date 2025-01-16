#version 440

// TODO: Dithering is not necessary with light colors. It is pretty much
//       necessary with dark colors due to premultiplied alpha to prevent
//       color banding. So, this should ideally be used only when the
//       background color is dark. When the build system starts supporting
//       defines to feed qsb, we can have the non-dithering version as well.
#define DITHERING

/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#extension GL_GOOGLE_include_directive : enable

#include "Common.glsl"

layout(location = 0) in vec2 qt_TexCoord0;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;

layout(std140, binding = 0) uniform buf {
  mat4 qt_Matrix;
  float qt_Opacity;
};


void main()
{
    vec4 texel = texture(source, qt_TexCoord0);

    fragColor = texel * qt_Opacity;

#ifdef DITHERING
    float r = rand(qt_TexCoord0) - 0.5;
    vec4 noise = vec4(r,r,r,r) * DITHERING_STRENGTH;
    fragColor += noise * step(DITHERING_CUTOFF, fragColor.a); // additive
#endif
}
