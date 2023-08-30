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

layout(location = 0) in vec2 coord;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float beginningFadeSize;
    float endFadePos;
};
layout(binding = 1) uniform sampler2D source;
layout(location = 1) in float pos;

void main() {
  lowp vec4 texel = texture(source, coord);

  // Note that the whole texel is multiplied instead
  // of only the alpha component because it must be
  // in premultiplied alpha format.
  texel *= (1.0 - smoothstep(endFadePos, 1.0, pos));
  texel *= (smoothstep(0.0, beginningFadeSize, pos));

  // We still need to respect the accumulated scene graph opacity:
  fragColor = texel * qt_Opacity;
}
