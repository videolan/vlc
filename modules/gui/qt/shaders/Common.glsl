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

/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

//blending formulas are taken from Qt's Blend.qml implementation

#define DITHERING_CUTOFF 0.01
#define DITHERING_STRENGTH 0.01

vec4 exclude(vec4 src, vec4 dst)
{
    return src + dst - 2.0 * src * dst;
}

vec4 fromPremult(vec4 color) {
    vec4 result = vec4(0.0);
    result.rgb = color.rgb / max(1.0/256.0, color.a);
    result.a = color.a;
    return result;
}

vec4 toPremult(vec4 color) {
    vec4 result = vec4(0.0);
    result.rbg = color.rbg * color.a;
    result.a = color.a;
    return result;
}

vec4 screen(vec4 color1, vec4 color2) {
    vec4 result = vec4(0.0);
    float a = max(color1.a, color1.a * color2.a);

    result.rgb = 1.0 - (vec3(1.0) - color1.rgb) * (vec3(1.0) - color2.rgb);

    result.rgb = mix(color1.rgb, result.rgb, color2.a);
    result.a = max(color1.a, color1.a * color2.a);
    return result;
}

vec4 multiply(vec4 color1, vec4 color2) {
    vec4 result = vec4(0.0);

    result.rgb = color1.rgb * color2.rgb;

    result.rgb = mix(color1.rgb, result.rgb, color2.a);
    result.a = max(color1.a, color1.a * color2.a);
    return result;
}

vec4 normal(vec4 color1, vec4 color2) {
    vec4 result = vec4(0.0);
    result.rgb = mix(color1.rgb, color2.rgb, color2.a);
    result.a = max(color1.a, color2.a);
    return result;
}

float rand(vec2 co){
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
