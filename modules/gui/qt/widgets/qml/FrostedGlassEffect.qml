/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

Rectangle {
    id: effect

    property variant source
    property variant sourceRect: undefined

    property bool active: true

    color: tint

    property color tint: VLCStyle.colors.banner
    property real tintStrength: 0.7
    property real noiseStrength: 0.02
    property real exclusionStrength: 0.09

    Item {
        anchors.fill: parent

        layer.enabled: effect.active
        layer.effect: Item {

            FastBlur {
                id: effect1
                anchors.fill: parent

                source: ShaderEffectSource {
                    sourceItem: effect.source
                    sourceRect: !!effect.sourceRect ? effect.sourceRect : effect.mapToItem(effect.source, Qt.rect(effect.x, effect.y, effect.width, effect.height))
                    visible: false
                }

                radius: 64
                visible: false
            }

            ShaderEffect {
                id: effect2

                property variant source: ShaderEffectSource {
                    sourceItem: effect1
                    visible: true
                }

                anchors.fill: parent

                visible: true

                property color tint: effect.tint
                property real  tintStrength: effect.tintStrength
                property real  noiseStrength: effect.noiseStrength
                property real  exclusionStrength: effect.exclusionStrength

                fragmentShader: "
                    uniform lowp sampler2D source; // this item
                    varying highp vec2 qt_TexCoord0;

                    uniform lowp vec4  tint;

                    uniform lowp float exclusionStrength;
                    uniform lowp float noiseStrength;
                    uniform lowp float tintStrength;


                    float rand(vec2 co){
                        return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
                    }


                    vec4 exclude(vec4 src, vec4 dst)
                    {
                        return src + dst - 2.0 * src * dst;
                    }

                    void main() {
                       float r = rand(qt_TexCoord0) - 0.5;
                       vec4 noise = vec4(r,r,r,1.0) * noiseStrength;
                       vec4 blured  = texture2D(source, qt_TexCoord0);

                       vec4 exclColor = vec4(exclusionStrength, exclusionStrength, exclusionStrength, 0.0);

                       blured = exclude(blured, exclColor);

                       gl_FragColor = mix(blured, tint, tintStrength) + noise;
                    }"
            }
        }
    }
}
