#version 440
/*****************************************************************************
 * Copyright (C) 2022 The Qt Company Ltd.
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
    // qt_Matrix and qt_Opacity must always be both present
    // if the built-in vertex shader is used.
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color;
    float relativeSizeX;
    float relativeSizeY;
    float spread;
};

float linearstep(float e0, float e1, float x)
{
    return clamp((x - e0) / (e1 - e0), 0.0, 1.0);
}

void main()
{
    float alpha =
        smoothstep(0.0, relativeSizeX, 0.5 - abs(0.5 - qt_TexCoord0.x)) *
        smoothstep(0.0, relativeSizeY, 0.5 - abs(0.5 - qt_TexCoord0.y));

    float spreadMultiplier = linearstep(spread, 1.0 - spread, alpha);
    fragColor = color * qt_Opacity * spreadMultiplier * spreadMultiplier;
}
