#version 440

// WARNING: This file must be in sync with DualKawaseBlur.frag
// TODO: Generate this shader at build time.
#define DOWNSAMPLE

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

layout(std140, binding = 0) uniform qt_buf {
    mat4 qt_Matrix;
    float qt_Opacity;

    vec4 normalRect; // unused, but Qt needs it as it used in first-pass vertex shader for sub-texturing (Qt bug?)
    vec2 sourceTextureSize;
    int radius;

#ifdef POSTPROCESS
    vec4 tint;
    float tintStrength;
    float exclusionStrength;
    float noiseStrength;
#endif
};

layout(binding = 1) uniform sampler2D source;

// "Dual Kawase"
// SIGGRAPH 2015, "Bandwidth Efficient Rendering", Marius Bjorge (ARM)
// https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_marius_2D00_notes.pdf
/// <dualkawaseblur>

#ifdef UPSAMPLE
#define SAMPLE upsample
vec4 upsample(vec2 uv, vec2 halfpixel)
{
    vec4 sum = fromPremult(texture(source, uv + vec2(-halfpixel.x * 2.0, 0.0)));
    sum += fromPremult(texture(source, uv + vec2(-halfpixel.x, halfpixel.y))) * 2.0;
    sum += fromPremult(texture(source, uv + vec2(0.0, halfpixel.y * 2.0)));
    sum += fromPremult(texture(source, uv + vec2(halfpixel.x, halfpixel.y))) * 2.0;
    sum += fromPremult(texture(source, uv + vec2(halfpixel.x * 2.0, 0.0)));
    sum += fromPremult(texture(source, uv + vec2(halfpixel.x, -halfpixel.y))) * 2.0;
    sum += fromPremult(texture(source, uv + vec2(0.0, -halfpixel.y * 2.0)));
    sum += fromPremult(texture(source, uv + vec2(-halfpixel.x, -halfpixel.y))) * 2.0;
    sum = sum / 12.0;
#ifdef POSTPROCESS
    return sum;
#else
    return toPremult(sum);
#endif
}
#endif

#ifdef DOWNSAMPLE
#define SAMPLE downsample
vec4 downsample(vec2 uv, vec2 halfpixel)
{
    vec4 sum = fromPremult(texture(source, uv)) * 4.0;
    sum += fromPremult(texture(source, uv - halfpixel.xy));
    sum += fromPremult(texture(source, uv + halfpixel.xy));
    sum += fromPremult(texture(source, uv + vec2(halfpixel.x, -halfpixel.y)));
    sum += fromPremult(texture(source, uv - vec2(halfpixel.x, -halfpixel.y)));
    sum = sum / 8.0;
#ifdef POSTPROCESS
    return sum;
#else
    return toPremult(sum);
#endif
}
#endif

/// </dualkawaseblur>

void main()
{
    // When `supportsAtlasTextures` is set, proper `uv` is provided, even for arbitrary
    // sub-textures, so we don't need to calculate here:
    vec2 uv = qt_TexCoord0;

    // We need to be careful to calculate the halfpixel properly for sub- and atlas textures:
    // If sourceTextureSize is sourced from QML, such as `Image`'s implicit size which normally
    // matches the texture size 1:1, if the texture is a sub-texture or atlas texture, the
    // texture size would not reflect the actual texture size. For that, we need to divide
    // `sourceTextureSize` by `qt_SubRect_source.zw` to get the actual texture size (which
    // means the atlas size, for example). This was the case in the first iteration, however
    // we started using a C++ utility class to get the texture size directly, so now we can
    // use it as is. The disadvantage is that the size needs to hop through the QML engine,
    // essentially SG -> QML -> SG (here), which may delay having the correct size here. If
    // we use GLSL 1.30 feature `textureSize()` instead, this would not be an issue. Currently
    // we can not do that because even though the shaders are written in GLSL 4.40, we target
    // as low as GLSL 1.20/ESSL 1.0. But maybe this is not a big deal, because if the size
    // (or texture altogether) changes, `QSGTextureProvider::textureChanged()` may need to
    // be processed in QML anyway (so the new size comes at the same time as the texture updates).
    // TODO: Ditch targeting GLSL 1.20/ESSL 1.0, and use `(radius - 0.5) / textureSize(source, 0)` instead.
    // TODO: This may be done in the vertex shader. I have not done that as this is a very simple
    //       calculation, and custom vertex shader in `ShaderEffect` breaks batching (which is not
    //       really important with the blur effect, so maybe it makes sense).
    vec2 halfpixel = (radius - 0.5) / sourceTextureSize;

    vec4 result = SAMPLE(uv, halfpixel);

#ifdef POSTPROCESS
    // Exclusion:
    vec4 exclColor = vec4(exclusionStrength, exclusionStrength, exclusionStrength, 0.0);
    result = exclude(result, exclColor);

    // Tint:
    result = mix(result, fromPremult(tint), tintStrength);

    // Noise:
    float r = rand(qt_TexCoord0) - 0.5;
    vec4 noise = vec4(r,r,r,1.0) * noiseStrength;
    result += noise;

    result = toPremult(result);
#endif

    fragColor = result * qt_Opacity; // premultiplied alpha
}
