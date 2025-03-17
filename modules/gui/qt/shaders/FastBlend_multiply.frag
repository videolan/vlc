#version 440

// WARNING: This file must be in sync with FastBlend.frag
// TODO: Generate this shader at build time.
#define MULTIPLY

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

layout(location = 0) out vec4 fragColor; // premultiplied

layout(std140, binding = 0) uniform buf {
  mat4 qt_Matrix;
  float qt_Opacity;
#if defined(MULTIPLY) || defined(SCREEN)
  float grayscale;
#else
  vec4 color; // premultiplied
#endif
};

void main() {
#if defined(ADDITIVE)
    fragColor = color * qt_Opacity;
    fragColor.a = 0.0;
#elif defined(MULTIPLY)
    fragColor.rgb = vec3(0.0, 0.0, 0.0);
    fragColor.a = (1.0 - (grayscale * qt_Opacity));
#elif defined(SCREEN)
    fragColor = vec4(grayscale, grayscale, grayscale, grayscale) * qt_Opacity;
#else
    fragColor = color * qt_Opacity;
#endif
}
