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

import VLC.Style
import VLC.Util

Item {
    id: root

    // Provide background color for sub-pixel font
    // rendering, even if it is not fully opaque.
    // If possible, provide a fully opaque color
    // so that layering is not used for the view,
    // which results lower memory consumption:
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

    // Fading edge effect provider when background is opaque:
    // This is an optimization, because layering is not used.
    component FadingEdgeRectangleForOpaqueBackground : Rectangle {
        id: fadingEdgeRectangle

        implicitHeight: root.fadeSize
        implicitWidth: root.fadeSize

        visible: (opacity > 0.0) && (root.backgroundColor.a > (1.0 - Number.EPSILON))

        opacity: enabled ? 1.0 : 0.0

        // Animating opacity is expected to be less expensive than animating gradient stop.
        Behavior on opacity {
            id: opacityBehavior

            enabled: false

            NumberAnimation {
                duration: VLCStyle.duration_short
                easing.type: Easing.InOutSine
            }
        }

        // This is used to prevent animating at initialization.
        Timer {
            interval: 50
            running: true
            onTriggered: {
                opacityBehavior.enabled = true
            }
        }

        required property color colorStop0
        required property color colorStop1

        // FIXME: Qt 6.2 switching orientation within `Gradient` is problematic
        gradient: (root.orientation === Qt.Horizontal) ? horizontalGradient
                                                       : verticalGradient

        Gradient {
            id: horizontalGradient

            orientation: Gradient.Horizontal

            GradientStop { position: 0.0; color: fadingEdgeRectangle.colorStop0; }
            GradientStop { position: 1.0; color: fadingEdgeRectangle.colorStop1; }
        }

        Gradient {
            id: verticalGradient

            orientation: Gradient.Vertical

            GradientStop { position: 0.0; color: fadingEdgeRectangle.colorStop0; }
            GradientStop { position: 1.0; color: fadingEdgeRectangle.colorStop1; }
        }
    }

    FadingEdgeRectangleForOpaqueBackground {
        anchors {
            top: shaderEffectSource.top
            left: shaderEffectSource.left

            bottom: (root.orientation === Qt.Horizontal) ? shaderEffectSource.bottom : undefined
            right: (root.orientation === Qt.Vertical) ? shaderEffectSource.right : undefined
        }

        enabled: root.enableBeginningFade
        colorStop0: root.backgroundColor
        colorStop1: "transparent"
    }

    FadingEdgeRectangleForOpaqueBackground {
        anchors {
            bottom: shaderEffectSource.bottom
            right: shaderEffectSource.right

            top: (root.orientation === Qt.Horizontal) ? shaderEffectSource.top : undefined
            left: (root.orientation === Qt.Vertical) ? shaderEffectSource.left : undefined
        }

        enabled: root.enableEndFade
        colorStop0: "transparent"
        colorStop1: root.backgroundColor
    }

    // Fading edge effect provider when background is not opaque:
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
                 (root.backgroundColor.a < 1.0) &&
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
                    Qt.callLater(effect._enableBeginningFadeBehavior)
                }
            }

            onEndFadeSizeChanged: {
                if (!endFadeBehavior.enabled) {
                    Qt.callLater(effect._enableEndFadeBehavior)
                }
            }

            function _enableBeginningFadeBehavior() {
                beginningFadeBehavior.enabled = true
            }

            function _enableEndFadeBehavior() {
                endFadeBehavior.enabled = true
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

            // TODO: Dithering is not necessary if background color is not dark.
            fragmentShader: "qrc:///shaders/FadingEdge.frag.qsb"
        }
    }
}
