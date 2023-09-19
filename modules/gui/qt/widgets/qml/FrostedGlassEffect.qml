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

import QtQuick
import Qt5Compat.GraphicalEffects

import "qrc:///style/"

// This item can be used as a layer effect.
// Make sure that the sampler name is set to "source" (default).
FastBlur {
    id: root

    radius: 64

    property bool blending: false

    property color tint: "transparent"
    property real tintStrength: Qt.colorEqual(tint, "transparent") ? 0.0 : 0.7
    property real noiseStrength: 0.02
    property real exclusionStrength: 0.09

    layer.enabled: true
    layer.effect: ShaderEffect {
        readonly property color tint: root.tint
        readonly property real tintStrength: root.tintStrength
        readonly property real noiseStrength: root.noiseStrength
        readonly property real exclusionStrength: root.exclusionStrength

        cullMode: ShaderEffect.BackFaceCulling

        blending: root.blending

        fragmentShader: "
            uniform lowp sampler2D source;
            varying highp vec2 qt_TexCoord0;

            uniform lowp float qt_Opacity;

            uniform lowp vec4 tint;

            uniform lowp float exclusionStrength;
            uniform lowp float noiseStrength;
            uniform lowp float tintStrength;

            highp float rand(highp vec2 co){
                return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
            }

            highp vec4 exclude(highp vec4 src, highp vec4 dst)
            {
                return src + dst - 2.0 * src * dst;
            }

            void main() {
               highp float r = rand(qt_TexCoord0) - 0.5;
               highp vec4 noise = vec4(r,r,r,1.0) * noiseStrength;
               highp vec4 blurred  = texture2D(source, qt_TexCoord0);

               highp vec4 exclColor = vec4(exclusionStrength, exclusionStrength, exclusionStrength, 0.0);

               blurred = exclude(blurred, exclColor);

               gl_FragColor = (mix(blurred, tint, tintStrength) + noise) * qt_Opacity;
            }"
    }
}
