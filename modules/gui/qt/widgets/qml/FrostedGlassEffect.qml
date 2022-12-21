/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

import QtQuick 2.11
import QtGraphicalEffects 1.0

import "qrc:///style/"

// This item can be used as a layer effect.
// Right now, it has the following limitations:
// * The blur effect is processed for the whole source,
//   even though it is only shown for the area denoted
//   by effectRect. This is caused by FastBlur not
//   accepting a source rectangle.
// * The source is sampled and displayed as a whole,
//   however it is stacked below of the blur effect
//   so it is only partly seen as intended.
// * As a corollary of the previous limitation, you should
//   always have a solid background for the source item.
//   otherwise, the effect can not work properly.
Item {
    id: effect

    property var source

    // Rectangular area where the effect should be applied:
    property alias effectRect: blurProxy.sourceRect

    property alias blurRadius: blurEffect.radius

    property color tint: "transparent"
    property real tintStrength: Qt.colorEqual(tint, "transparent") ? 0.0 : 0.7
    property real noiseStrength: 0.02
    property real exclusionStrength: 0.09

    ShaderEffect {
        anchors.fill: parent

        property alias source: effect.source

        cullMode: ShaderEffect.BackFaceCulling
    }

    FastBlur {
        id: blurEffect

        anchors.fill: parent

        source: effect.source

        radius: 64

        visible: false
    }

    ShaderEffectSource {
        id: blurProxy

        x: Math.floor(sourceRect.x)
        y: Math.floor(sourceRect.y)
        width: sourceRect.width > 0 ? Math.ceil(sourceRect.width)
                                    : implicitWidth
        height: sourceRect.height > 0 ? Math.ceil(sourceRect.height)
                                      : implicitHeight

        implicitWidth: Math.ceil(parent.width)
        implicitHeight: Math.ceil(parent.height)

        sourceItem: blurEffect
        recursive: false
        samples: 0
        smooth: false

        mipmap: false

        layer.enabled: true
        layer.effect: ShaderEffect {
            readonly property color tint: effect.tint
            readonly property real tintStrength: effect.tintStrength
            readonly property real noiseStrength: effect.noiseStrength
            readonly property real exclusionStrength: effect.exclusionStrength

            cullMode: ShaderEffect.BackFaceCulling

            fragmentShader: "
                uniform lowp sampler2D source; // this item
                varying highp vec2 qt_TexCoord0;

                uniform lowp float qt_Opacity;

                uniform lowp vec4  tint;

                uniform lowp float exclusionStrength;
                uniform lowp float noiseStrength;
                uniform lowp float tintStrength;

                mediump float rand(highp vec2 co){
                    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
                }

                mediump vec4 exclude(mediump vec4 src, mediump vec4 dst)
                {
                    return src + dst - 2.0 * src * dst;
                }

                void main() {
                   mediump float r = rand(qt_TexCoord0) - 0.5;
                   mediump vec4 noise = vec4(r,r,r,1.0) * noiseStrength;
                   mediump vec4 blurred  = texture2D(source, qt_TexCoord0);

                   mediump vec4 exclColor = vec4(exclusionStrength, exclusionStrength, exclusionStrength, 0.0);

                   blurred = exclude(blurred, exclColor);

                   gl_FragColor = (mix(blurred, tint, tintStrength) + noise) * qt_Opacity;
                }"
        }
    }
}
