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

layout(location = 1) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;

    // QTBUG-50493, use array when supported:
    vec4 discardRect0;
    vec4 discardRect1;
    vec4 discardRect2;
    vec4 discardRect3;
};

layout(binding = 1) uniform sampler2D source;

void rectFilter(vec4 rect)
{
    if ((qt_TexCoord0.x >= rect.x && qt_TexCoord0.x <= rect.z) &&
        (qt_TexCoord0.y >= rect.y && qt_TexCoord0.y <= rect.w))
            discard;
}

void main() {
    rectFilter(discardRect0);
    rectFilter(discardRect1);
    rectFilter(discardRect2);
    rectFilter(discardRect3);

    highp vec4 texel = texture(source, qt_TexCoord0);

    fragColor = texel * qt_Opacity;
}
