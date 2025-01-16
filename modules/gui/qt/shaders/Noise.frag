#version 440

/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

layout(std140, binding = 0) uniform buf {
  mat4 qt_Matrix;
  float qt_Opacity;
  float strength;
};

void main() {
   float r = rand(qt_TexCoord0) - 0.5;
   vec4 noise = vec4(r,r,r,1.0) * strength;
   fragColor = noise * qt_Opacity;
   // Noise should use additive blending (S + D) instead of the default source-over
   // blending (S + D * (1 - S.a)). For that, set alpha to 0:
   // Since it is premultiplied, alpha is already factored in the other channels
   // and should be still respected. "If blending is enabled, source-over blending
   // is used. However, additive blending can be achieved by outputting zero in the
   // alpha channel."
   fragColor.a = 0.0;
}
