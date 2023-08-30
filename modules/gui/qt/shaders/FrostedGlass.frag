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

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
  mat4 qt_Matrix;
  float qt_Opacity;
  vec4 tint;
  float exclusionStrength;
  float noiseStrength;
  float tintStrength;
};
layout(binding = 1) uniform sampler2D source;

float rand(vec2 co){
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 exclude(vec4 src, vec4 dst)
{
    return src + dst - 2.0 * src * dst;
}

void main() {
   float r = rand(qt_TexCoord0) - 0.5;
   vec4 noise = vec4(r,r,r,1.0) * noiseStrength;
   vec4 blurred  = texture(source, qt_TexCoord0);

   vec4 exclColor = vec4(exclusionStrength, exclusionStrength, exclusionStrength, 0.0);

   blurred = exclude(blurred, exclColor);

   fragColor = (mix(blurred, tint, tintStrength) + noise) * qt_Opacity;
}
