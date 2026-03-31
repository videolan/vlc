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
import QtQuick.Window

import VLC.MainInterface
import VLC.Util

// This item can be used as a layer effect.
// The purpose of this item is to apply an effect to a partial
// area of an item without re-rendering the source item multiple
// times.
Item {
    id: root

    // source must be a texture provider Item.
    // If layer is enabled, the source (parent) will be a texture provider
    // and automatically set by Qt.
    // Some items, such as Image and ShaderEffectSource can be implicitly
    // a texture provider without creating an extra layer.
    // Make sure that the sampler name is set to "source" (default) if
    // this is used as a layer effect.
    property alias source: textureProviderIndirection.source

    // Rectangular area where the effect should be applied:
    property alias effectRect: textureProviderIndirection.effectRect

    // Not mandatory to provide, but when feasible (such as, effect is not
    // an isolated inner area), provide it for optimization. When not provided,
    // the source visual is going to cover the whole area, and if source is not
    // opaque (blending is enabled), the effect part is going to be filtered in
    // fragment shader, which is more expensive than sub-texturing by means of
    // this property. That being said, when source is opaque (blending is off),
    // then providing this may serve no purpose due to early fragment test as
    // long as the scene graph uses depth buffer (default). For that reason, it
    // is recommended to provide this only in non-opaque cases (blending is set):
    property rect sourceVisualRect

    property Item effect
    property string samplerName: "source"

    // Enable blending if source or the effect is not opaque.
    // This comes with a performance penalty.
    blending: false

    property alias blending: sourceProxy.blending

    // Display scale may be a fractional size, but textures can not. Pixel aligned
    // aligns the visual size to the nearest multiple of number, depending on
    // the window/screen fraction, so that the layer texture can be displayed
    // without stretching. For example, if the display scale is 1.25, the
    // visual size would be aligned up to the nearest multiple of 4. Note that
    // using this property is going to make the visual to deviate from the
    // size intended, which may increase greatly depending on the fraction
    // of the display scale. Also note that if the QML item size is fractional
    // itself, it is ceiled regardless of the display scale or this property.
    // This setting applies to both the source and the effect visuals, but
    // is only relevant for the source visual if `sourceVisualRect` is
    // provided.
    property bool pixelAlignedForDPR: true

    property real _eDPR: MainCtx.effectiveDevicePixelRatio(Window.window) || 1.0
    readonly property int _alignNumber: pixelAlignedForDPR ? Helpers.denominatorForFloat(_eDPR) : 1

    Connections {
        target: MainCtx

        function onIntfDevicePixelRatioChanged() {
            root._eDPR = MainCtx.effectiveDevicePixelRatio(root.Window.window) || 1.0
        }
    }

    // This item displays the item with the rect denoted by effectRect
    // being transparent (only when blending is set):
    ShaderEffect {
        id: sourceProxy

        x: root.sourceVisualRect.x
        y: root.sourceVisualRect.y
        width: useSubTexture ? Helpers.alignUp(root.sourceVisualRect.width, root._alignNumber) : parent.width
        height: useSubTexture ? Helpers.alignUp(root.sourceVisualRect.height, root._alignNumber) : parent.height

        smooth: false

        // WARNING: Switching the source should be fine, but old Qt (Qt 6.2.13) seems to break the interface
        //          in this case. The indirection does not use sub-rect if it is not relevant, so in this
        //          case it would act as a dummy indirection to prevent the Qt bug.
        readonly property Item source: ((MainCtx.qtVersion() < MainCtx.qtVersionCheck(6, 4, 2)) || useSubTexture) ? sourceVisualTextureProviderIndirection : root.source

        readonly property rect discardRect: {
            if (blending && !useSubTexture)
                return Qt.rect(textureProviderIndirection.x / root.width,
                               textureProviderIndirection.y / root.height,
                               (textureProviderIndirection.x + textureProviderIndirection.width) / root.width,
                               (textureProviderIndirection.y + textureProviderIndirection.height) / root.height)
            else // If blending is not enabled, no need to make the normalization calculations
                return Qt.rect(0, 0, 0, 0)
        }

        readonly property bool useSubTexture: (root.sourceVisualRect.width > 0.0 && root.sourceVisualRect.height > 0.0)

        supportsAtlasTextures: true

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        fragmentShader: (discardRect.width > 0.0 && discardRect.height > 0.0) ? "qrc:///shaders/RectFilter.frag.qsb" : ""

        TextureProviderIndirection {
            id: sourceVisualTextureProviderIndirection
            source: root.source

            // If the effect is in a isolated inner area, filtering is necessary. Otherwise, we can simply
            // use sub-texturing for the source itself as well (we already use sub-texture for the effect area).
            textureSubRect: (sourceProxy.useSubTexture) ? Qt.rect(sourceProxy.x * root._eDPR,
                                                                  sourceProxy.y * root._eDPR,
                                                                  sourceProxy.width * root._eDPR,
                                                                  sourceProxy.height * root._eDPR) : undefined
        }
    }

    // We use texture provider that uses QSGTextureView.
    // QSGTextureView is able to denote a viewport that
    // covers a certain area in the source texture.
    // This way, we don't need to have another layer just
    // to clip the source texture.
    TextureProviderIndirection {
        id: textureProviderIndirection

        x: effectRect.x
        y: effectRect.y
        width: Helpers.alignUp(effectRect.width, root._alignNumber)
        height: Helpers.alignUp(effectRect.height, root._alignNumber)

        readonly property Item sourceItem: source?.sourceItem ?? source

        property rect effectRect

        textureSubRect: Qt.rect(x * root._eDPR,
                                y * root._eDPR,
                                width * root._eDPR,
                                height * root._eDPR)

        readonly property bool effectAcceptsSourceRect: (typeof root.effect?.sourceRect !== "undefined") // typeof `rect` is "object"

        // Effect's source is sub-texture through the texture provider, as long as the effect does not accept setting `sourceRect`.
        // Unlike Qt's own effects, our `DualKawaseBlur` supports that. Note that we are not using a `Loader` to not load the
        // indirection in case it is accepted by the effect, since its overhead is minimal. Also note that the texture provider
        // is only created when `textureProvider()` is called, such that when this indirection is actually used (for example as
        // a shader effect source).
        Binding {
            target: root.effect
            property: root.samplerName
            value: textureProviderIndirection.effectAcceptsSourceRect ? root.source : textureProviderIndirection
        }

        Binding {
            target: root.effect
            property: "sourceRect"
            when: textureProviderIndirection.effectAcceptsSourceRect
            value: textureProviderIndirection.textureSubRect
        }

        // Adjust the blending. Currently MultiEffect/FastBlur does not
        // support adjusting it:
        Binding {
            target: root.effect
            when: root.effect && (typeof root.effect.blending === "boolean")
            property: "blending"
            value: root.blending
        }

        // Positioning:
        Binding {
            target: root.effect
            property: "parent"
            value: root
        }

        Binding {
            target: root.effect
            property: "anchors.fill"
            value: textureProviderIndirection
        }
    }
}
