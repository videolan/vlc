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

Item {
    id: effect

    property Item source
    property rect sourceRect: mapToItem(source, x, y, width, height)

    property bool recursive: false
    property real blurRadius: 64
    property color tint

    property real tintStrength: 0.7
    property real noiseStrength: 0.02
    property real exclusionStrength: 0.09

    readonly property bool effectAvailable: (GraphicsInfo.shaderType === GraphicsInfo.GLSL) &&
                                            (GraphicsInfo.shaderSourceType & GraphicsInfo.ShaderSourceString)

    opacity: source.opacity

    Loader {
        id: loader

        anchors.fill: parent

        sourceComponent: effect.effectAvailable ? effectComponent : rectComponent

        Component {
            id: rectComponent

            Rectangle {
                color: effect.tint
            }
        }

        Component {
            id: effectComponent

            FastBlur {
                id: blurEffect

                source: ShaderEffectSource {
                    id: effectSource
                    sourceItem: effect.source
                    sourceRect: effect.sourceRect
                    visible: false
                    samples: 0
                }

                radius: effect.blurRadius

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

    }
}
