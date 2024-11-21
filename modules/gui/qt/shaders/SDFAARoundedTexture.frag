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
    vec4 qt_SubRect_source;
    vec2 size;
    float radiusTopRight;
    float radiusBottomRight;
    float radiusTopLeft;
    float radiusBottomLeft;
    float softEdgeMin;
    float softEdgeMax;
};

layout(binding = 1) uniform sampler2D source;

float sdRoundBox( in vec2 p, in vec2 b, in vec4 r )
{
    // Pending actual implementation
    return 0.0;
}

void main()
{
    // The signed distance function works when the primitive is centered.
    // If the texture is in the atlas, this condition is not satisfied.
    // Therefore, we have to normalize the coordinate for the distance
    // function to [0, 1]:
    vec2 normalCoord = vec2(1.0, 1.0) / (qt_SubRect_source.zw) * (qt_TexCoord0 - (qt_SubRect_source.zw + qt_SubRect_source.xy)) + vec2(1.0, 1.0);

    vec2 p = (size.xy * ((2.0 * normalCoord) - 1)) / size.y;
    // Signed distance:
    float dist = sdRoundBox(p, vec2(size.x / size.y, 1.0), vec4(radiusTopRight, radiusBottomRight, radiusTopLeft, radiusBottomLeft));

    vec4 texel = texture(source, qt_TexCoord0);

    // Soften the outline, as recommended by the Valve paper, using smoothstep:
    // "Improved Alpha-Tested Magnification for Vector Textures and Special Effects"
    // NOTE: The whole texel is multiplied, because of premultiplied alpha.
    texel *= 1.0 - smoothstep(softEdgeMin, softEdgeMax, dist);

    fragColor = texel * qt_Opacity;
}
