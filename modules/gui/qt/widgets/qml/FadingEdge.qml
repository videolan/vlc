/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers

Item {
    id: root

    // backgroundColor is only needed for sub-pixel
    // font rendering. Or, if the background color
    // needs to be known during rendering in general.
    // Ideally it should be fully opaque, but it is
    // still better than not providing any color
    // information.
    property alias backgroundColor: backgroundRect.color

    property alias sourceItem: shaderEffectSource.sourceItem

    property real beginningMargin
    property real endMargin

    property real sourceX
    property real sourceY

    property int orientation: Qt.Vertical

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    property real fadeSize: VLCStyle.margin_normal

    readonly property bool effectCompatible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

    Rectangle {
        id: backgroundRect

        parent: root.sourceItem

        anchors.fill: parent

        color: "transparent"

        z: -100

        visible: shaderEffectSource.visible && (color.a > 0.0)
    }

    ShaderEffectSource {
        id: shaderEffectSource

        anchors {
            top: parent.top
            left: parent.left
            leftMargin: (root.orientation === Qt.Horizontal ? -root.beginningMargin : 0)
            topMargin: (root.orientation === Qt.Vertical ? -root.beginningMargin : 0)
        }

        implicitWidth: Math.ceil(parent.width + (root.orientation === Qt.Horizontal ? (root.beginningMargin + root.endMargin) : 0))
        implicitHeight: Math.ceil(parent.height + (root.orientation === Qt.Vertical ? (root.beginningMargin + root.endMargin) : 0))

        sourceRect: Qt.rect(root.sourceX - (root.orientation === Qt.Horizontal ? root.beginningMargin : 0),
                            root.sourceY - (root.orientation === Qt.Vertical ? root.beginningMargin : 0),
                            width,
                            height)

        // Make sure sourceItem is not rendered twice:
        hideSource: visible

        smooth: false

        visible: effectCompatible &&
                 ((root.enableBeginningFade || root.enableEndFade) ||
                 ((shaderEffect) && (shaderEffect.beginningFadeSize > 0 || shaderEffect.endFadeSize > 0)))

        property ShaderEffect shaderEffect

        layer.enabled: true
        layer.effect: ShaderEffect {
            // It makes sense to use the effect for only in the fading part.
            // However, it would complicate things in the QML side. As it
            // would require two additional items, as well as two more texture
            // allocations.
            // Given the shading done here is not complex, this is not done.
            // Applying the texture and the effect is done in one step.

            id: effect

            readonly property bool vertical: (root.orientation === Qt.Vertical)
            readonly property real normalFadeSize: root.fadeSize / (vertical ? height : width)

            property real beginningFadeSize: root.enableBeginningFade ? normalFadeSize : 0
            property real endFadeSize: root.enableEndFade ? normalFadeSize : 0
            readonly property real endFadePos: 1.0 - endFadeSize

            onBeginningFadeSizeChanged: {
                if (!beginningFadeBehavior.enabled) {
                    Qt.callLater(() => {
                        beginningFadeBehavior.enabled = true
                    })
                }
            }

            onEndFadeSizeChanged: {
                if (!endFadeBehavior.enabled) {
                    Qt.callLater(() => {
                        endFadeBehavior.enabled = true
                    })
                }
            }

            Component.onCompleted: {
                console.assert(shaderEffectSource.shaderEffect === null)
                shaderEffectSource.shaderEffect = this
            }

            Component.onDestruction: {
                console.assert(shaderEffectSource.shaderEffect === this)
                shaderEffectSource.shaderEffect = null
            }

            component FadeBehavior : Behavior {
                enabled: false

                // Qt Bug: UniformAnimator does not work...
                // FIXME: Is it fixed with Qt 6?
                NumberAnimation {
                    duration: VLCStyle.duration_veryShort
                    easing.type: Easing.InOutSine
                }
            }

            FadeBehavior on beginningFadeSize {
                id: beginningFadeBehavior
            }

            FadeBehavior on endFadeSize {
                id: endFadeBehavior
            }

            // Atlas textures can be supported
            // but in this use case it is not
            // necessary as the layer texture
            // can not be placed in the atlas.
            supportsAtlasTextures: false

            // cullMode: ShaderEffect.BackFaceCulling // Does not work sometimes. Qt bug?

            vertexShader: "qrc:///shaders/FadingEdge%1.vert.qsb".arg(vertical ? "Y" : "X")
            fragmentShader: "qrc:///shaders/FadingEdge.frag.qsb"
        }
    }
}
