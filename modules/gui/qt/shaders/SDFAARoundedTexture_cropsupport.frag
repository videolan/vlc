#version 440

// TODO: Build system support for preprocessor defines, like CMake's `qt6_add_shaders()`.
// NOTE: Currently the build system does not support defines for the shaders,
//       So, the following is imported manually from SDFAARoundedTexture.frag.
// FIXME: Remove this file when build system starts supporting defines for the
//        shaders.

#define CROP_SUPPORT

// WARNING: The contents of this file must be in sync with SDFAARoundedTexture.frag
//          for maintenance purposes. IF YOU EDIT THIS FILE, MAKE SURE TO DO THE
//          SAME IN SDFAARoundedTexture.frag.

#define ANTIALIASING
#define BACKGROUND_SUPPORT
#define CUSTOM_SOFTEDGE

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

/*****************************************************************************
 * Copyright (C) 2015 Inigo Quilez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *****************************************************************************/

layout(location = 0) in vec2 qt_TexCoord0;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 qt_SubRect_source;
    vec2 size;
#ifdef CROP_SUPPORT
    vec2 cropRate;
#endif
#ifdef BACKGROUND_SUPPORT
    vec4 backgroundColor;
#endif
    float radiusTopRight;
    float radiusBottomRight;
    float radiusTopLeft;
    float radiusBottomLeft;
#ifdef CUSTOM_SOFTEDGE
    float softEdgeMin;
    float softEdgeMax;
#endif
};

layout(binding = 1) uniform sampler2D source;

// Signed distance function by Inigo Quilez (https://iquilezles.org/articles/distfunctions2d)
// b.x = width
// b.y = height
// r.x = roundness top-right
// r.y = roundness boottom-right
// r.z = roundness top-left
// r.w = roundness bottom-left
float sdRoundBox( in vec2 p, in vec2 b, in vec4 r )
{
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
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

#ifdef CROP_SUPPORT
    vec2 texCoord;

    // if (cropRate.x > 0.0)
    {
        float normalCropRate = qt_SubRect_source.z * cropRate.x;

        float k = qt_SubRect_source.z + qt_SubRect_source.x - normalCropRate;
        float l = qt_SubRect_source.x + normalCropRate;

        texCoord.x = (k - l) / (qt_SubRect_source.z) * (qt_TexCoord0.x - qt_SubRect_source.x) + l;
    }
    // else { texCoord.x = qt_TexCoord0.x; }

    // if (cropRate.y > 0.0)
    {
        float normalCropRate = qt_SubRect_source.w * cropRate.y;

        float k = qt_SubRect_source.w + qt_SubRect_source.y - normalCropRate;
        float l = qt_SubRect_source.y + normalCropRate;

        texCoord.y = (k - l) / (qt_SubRect_source.w) * (qt_TexCoord0.y - qt_SubRect_source.y) + l;
    }
    // else { texCoord.y = qt_TexCoord0.y; }

    vec4 texel = texture(source, texCoord);
#else
    vec4 texel = texture(source, qt_TexCoord0);
#endif

#ifdef BACKGROUND_SUPPORT
    // Source over blending (S + D * (1 - S.a)):
    texel = texel + backgroundColor * (1.0 - texel.a);
#endif

#ifdef ANTIALIASING
#ifndef CUSTOM_SOFTEDGE
    float softEdgeMax = fwidth(dist) * 0.75;
    float softEdgeMin = -softEdgeMax;
#endif
    // Breathing room (shrink):
    dist += softEdgeMax;

    // Soften the outline, as recommended by the Valve paper, using smoothstep:
    // "Improved Alpha-Tested Magnification for Vector Textures and Special Effects"
    // NOTE: The whole texel is multiplied, because of premultiplied alpha.
    float factor = smoothstep(softEdgeMin, softEdgeMax, dist);
#else
    float factor = step(0.0, dist);
#endif
    texel *= 1.0 - factor;

    fragColor = texel * qt_Opacity;
}
