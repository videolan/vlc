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
import QtQuick.Window

import VLC.MainInterface
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

    property Item sourceItem

    property real beginningMargin
    property real endMargin

    property real sourceX
    property real sourceY

    property int orientation: Qt.Vertical

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    property real fadeSize: VLCStyle.margin_normal

    // Display scale may be a fractional size, but textures can not. Pixel aligned
    // aligns the visual size to the nearest multiple of number, depending on
    // the window/screen fraction, so that the layer texture can be displayed
    // without stretching. For example, if the display scale is 1.25, the
    // visual size would be aligned up to the nearest multiple of 4. Note that
    // using this property is going to make the visual to deviate from the
    // size intended, which may increase greatly depending on the fraction
    // of the display scale. Also note that if the QML item size is fractional
    // itself, it is ceiled regardless of the display scale or this property.
    property bool pixelAlignedForDPR: true

    readonly property bool effectCompatible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

    readonly property bool implicitClipping: !!_shaderEffect?.visible

    property bool useLayering: effectCompatible && (enforceClipping || backgroundColor.a < 1.0)

    // Sometimes `Item::clip` may not be suitable to use with item views,
    // this includes, but not limited to, when display margins are set.
    // In this case, `enforceClipping` may be set. Note that setting this
    // property does not guarantee that clipping is done, it is advised
    // to check `implicitClipping` to know if clipping is actually done.
    // NOTE: `useLayering` must be set for this to be respected, by
    //       default it is set when `enforceClipping` is set.
    // TODO: Get rid of this once we can adjust the clip rect directly
    //       in QML (currently requires overriding `QQuickItem::clipRect()`).
    property bool enforceClipping: false

    property Item _shaderEffectSource
    property Item _shaderEffect

    onUseLayeringChanged: {
        if (useLayering) {
            // We create the items needed for layer once it is needed, and keep it.
            // Note that, while we keep the items, the layer texture should still
            // be released if `useLayer` becomes `false` after that (since we set
            // `sourceItem` to `null` while `live` is set).

            if (!_shaderEffectSource) {
                _shaderEffectSource = shaderEffectSourceComponent.createObject(positionerParent)
            }

            if (!_shaderEffect) {
                _shaderEffect = shaderEffectComponent.createObject(positionerParent)
            }
        }
    }

    Rectangle {
        id: backgroundRect

        parent: root.sourceItem

        anchors.fill: parent

        color: "transparent"

        z: -100

        visible: !!root._shaderEffectSource?.visible && (color.a > 0.0)
    }

    // Fading edge effect provider when background is opaque:
    // This is an optimization, because layering is not used.
    component FadingEdgeRectangleForOpaqueBackground : Rectangle {
        id: fadingEdgeRectangle

        implicitHeight: root.fadeSize
        implicitWidth: root.fadeSize

        // NOTE: `OpacityAnimator` updates the opacity once the animation finishes.
        visible: (opacity > 0.0 || enabled) && !root.useLayering

        opacity: enabled ? 1.0 : 0.0

        // Animating opacity is expected to be less expensive than animating gradient stop.
        Behavior on opacity {
            id: opacityBehavior

            enabled: false

            OpacityAnimator {
                duration: VLCStyle.duration_short
                easing.type: Easing.InOutSine
            }
        }

        Component.onCompleted: {
            // This is used to prevent animating at initialization:
            MainCtx.setTimeout(() => {
                opacityBehavior.enabled = true
            }, 50, [], fadingEdgeRectangle)
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

    Item {
        id: positionerParent

        anchors {
            fill: parent
            leftMargin: (root.orientation === Qt.Horizontal ? -root.beginningMargin : 0)
            rightMargin: (root.orientation === Qt.Horizontal ? -root.endMargin : 0)
            topMargin: (root.orientation === Qt.Vertical ? -root.beginningMargin : 0)
            bottomMargin: (root.orientation === Qt.Vertical ? -root.endMargin : 0)
        }

        FadingEdgeRectangleForOpaqueBackground {
            anchors {
                top: parent.top
                left: parent.left

                bottom: (root.orientation === Qt.Horizontal) ? parent.bottom : undefined
                right: (root.orientation === Qt.Vertical) ? parent.right : undefined
            }

            enabled: root.enableBeginningFade
            colorStop0: root.backgroundColor
            colorStop1: "transparent"
        }

        FadingEdgeRectangleForOpaqueBackground {
            anchors {
                bottom: parent.bottom
                right: parent.right

                top: (root.orientation === Qt.Horizontal) ? parent.top : undefined
                left: (root.orientation === Qt.Vertical) ? parent.left : undefined
            }

            enabled: root.enableEndFade
            colorStop0: "transparent"
            colorStop1: root.backgroundColor
        }
    }

    Component {
        id: shaderEffectSourceComponent

        // Fading edge effect provider when background is not opaque:
        ShaderEffectSource {
            id: shaderEffectSource

            anchors.top: parent.top
            anchors.left: parent.left

            property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window) || 1.0
            readonly property int alignNumber: pixelAlignedForDPR ? Helpers.denominatorForFloat(eDPR) : 1

            Connections {
                target: MainCtx

                function onIntfDevicePixelRatioChanged() {
                    shaderEffectSource.eDPR = MainCtx.effectiveDevicePixelRatio(shaderEffectSource.Window.window) || 1.0
                }
            }

            implicitWidth: Helpers.alignUp((parent.width + (root.orientation === Qt.Horizontal ? (root.beginningMargin + root.endMargin) : 0)), alignNumber)
            implicitHeight: Helpers.alignUp((parent.height + (root.orientation === Qt.Vertical ? (root.beginningMargin + root.endMargin) : 0)), alignNumber)

            // When `live` is set, Qt releases the layer when `sourceItem` becomes `null`. For that reason
            // we do not need to set `parent` to null. The important thing is to get rid  of the layer
            // texture, we don't need to care about getting rid of the texture provider. See
            // `QQuickShaderEffectSource::updatePaintNode()` and `QSGRhiLayer::setItem()`.
            sourceItem: root.useLayering ? root.sourceItem : null

            sourceRect: Qt.rect(root.sourceX - (root.orientation === Qt.Horizontal ? root.beginningMargin : 0),
                                root.sourceY - (root.orientation === Qt.Vertical ? root.beginningMargin : 0),
                                width, // Texture width is (width * dpr)
                                height) // Texture height is (height * dpr)

            // Make sure sourceItem is not rendered twice:
            hideSource: root._shaderEffect && (root._shaderEffect.visible && root._shaderEffect.source === this)

            visible: false

            smooth: false
        }
    }

    Component {
        id: shaderEffectComponent

        ShaderEffect {
            // It makes sense to use the effect for only in the fading part.
            // However, it would complicate things in the QML side. As it
            // would require two additional items, as well as two more texture
            // allocations.
            // Given the shading done here is not complex, this is not done.
            // Applying the texture and the effect is done in one step.

            id: effect

            anchors.fill: root._shaderEffectSource

            readonly property Item source: root._shaderEffectSource

            readonly property bool vertical: (root.orientation === Qt.Vertical)
            readonly property real normalFadeSize: root.fadeSize / (vertical ? height : width)

            property real beginningFadeSize: root.enableBeginningFade ? normalFadeSize : 0
            property real endFadeSize: root.enableEndFade ? normalFadeSize : 0
            readonly property real endFadePos: 1.0 - endFadeSize

            visible: root.useLayering &&
                     (root.enforceClipping ||
                      ((root.enableBeginningFade || root.enableEndFade) ||
                       (beginningFadeSize > 0 || endFadeSize > 0)))

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
