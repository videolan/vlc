#version 440

// WARNING: This file must be in sync with RoundedRectangleShadow.frag
// TODO: Generate this shader at build time.
#define HOLLOW

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

// This is provided by the default vertex shader even when there is no texture:
layout(location = 0) in vec2 qt_TexCoord0;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
  mat4 qt_Matrix;
  float qt_Opacity;

  float blurRadius;
  float radius;
  float compensationFactor;

  vec2 size;
  vec4 color;
};

// Evan Wallace's Fast Rounded Rectangle Shadows
// https://madebyevan.com/shaders/fast-rounded-rectangle-shadows/
// License: CC0 (http://creativecommons.org/publicdomain/zero/1.0/)
/// <roundedboxshadow>

// A standard gaussian function, used for weighting samples
float gaussian(float x, float sigma) {
  const float pi = 3.141592653589793;
  return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * pi) * sigma);
}

// This approximates the error function, needed for the gaussian integral
vec2 erf(vec2 x) {
  vec2 s = sign(x), a = abs(x);
  x = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
  x *= x;
  return s - s / (x * x);
}

// Return the blurred mask along the x dimension
float roundedBoxShadowX(float x, float y, float sigma, float corner, vec2 halfSize) {
  float delta = min(halfSize.y - corner - abs(y), 0.0);
  float curved = halfSize.x - corner + sqrt(max(0.0, corner * corner - delta * delta));
  vec2 integral = 0.5 + 0.5 * erf((x + vec2(-curved, curved)) * (sqrt(0.5) / sigma));
  return integral.y - integral.x;
}

// Return the mask for the shadow of a box from lower to upper
float roundedBoxShadow(vec2 lower, vec2 upper, vec2 point, float sigma, float corner) {
  // Center everything to make the math easier
  vec2 center = (lower + upper) * 0.5;
  vec2 halfSize = (upper - lower) * 0.5;
  point -= center;

  // The signal is only non-zero in a limited range, so don't waste samples
  float low = point.y - halfSize.y;
  float high = point.y + halfSize.y;
  float start = clamp(-3.0 * sigma, low, high);
  float end = clamp(3.0 * sigma, low, high);

  // Accumulate samples (we can get away with surprisingly few samples)
  float step = (end - start) / 4.0;
  float y = start + step * 0.5;
  float value = 0.0;
  for (int i = 0; i < 4; i++) {
    value += roundedBoxShadowX(point.x, point.y - y, sigma, corner, halfSize) * gaussian(y, sigma) * step;
    y += step;
  }

  return value;
}

/// </roundedboxshadow>

void main()
{
    vec2 compensatedOffset = vec2(blurRadius, blurRadius) * compensationFactor;
    vec2 denormalCoord = size * qt_TexCoord0;

#ifdef HOLLOW
    if (denormalCoord.x >= compensatedOffset.x && denormalCoord.x <= (size.x - compensatedOffset.x) &&
        denormalCoord.y >= compensatedOffset.y && denormalCoord.y <= (size.y - compensatedOffset.y))
      discard;
#endif

    float shadow = roundedBoxShadow(compensatedOffset,
                                    size - compensatedOffset,
                                    denormalCoord,
                                    blurRadius,
                                    radius);

    fragColor = color * shadow * qt_Opacity; // premultiplied

#ifdef DITHERING
    float r = rand(qt_TexCoord0) - 0.5;
    vec4 noise = vec4(r,r,r,r) * DITHERING_STRENGTH;
    fragColor += noise * step(DITHERING_CUTOFF, fragColor.a); // additive
#endif
}
