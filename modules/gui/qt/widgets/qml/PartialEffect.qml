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
import VLC.Widgets as Widgets

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
    property alias source: textureProviderItem.source

    // Rectangular area where the effect should be applied:
    property alias effectRect: textureProviderItem.effectRect

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
    property alias blending: sourceProxy.blending

    // This item displays the item with the rect denoted by effectRect
    // being transparent (only when blending is set):
    ShaderEffect {
        id: sourceProxy

        x: root.sourceVisualRect.x
        y: root.sourceVisualRect.y
        width: useSubTexture ? root.sourceVisualRect.width : parent.width
        height: useSubTexture ? root.sourceVisualRect.height : parent.height

        blending: false

        readonly property Item source: useSubTexture ? sourceVisualTextureProviderItem : root.source

        readonly property rect discardRect: {
            if (blending && !useSubTexture)
                return Qt.rect(textureProviderItem.x / root.width,
                               textureProviderItem.y / root.height,
                               (textureProviderItem.x + textureProviderItem.width) / root.width,
                               (textureProviderItem.y + textureProviderItem.height) / root.height)
            else // If blending is not enabled, no need to make the normalization calculations
                return Qt.rect(0, 0, 0, 0)
        }

        readonly property bool useSubTexture: (root.sourceVisualRect.width > 0.0 && root.sourceVisualRect.height > 0.0)

        supportsAtlasTextures: true

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        fragmentShader: (discardRect.width > 0.0 && discardRect.height > 0.0) ? "qrc:///shaders/RectFilter.frag.qsb" : ""

        Widgets.TextureProviderItem {
            id: sourceVisualTextureProviderItem
            source: root.source

            // If the effect is in a isolated inner area, filtering is necessary. Otherwise, we can simply
            // use sub-texturing for the source itself as well (we already use sub-texture for the effect area).
            textureSubRect: (sourceProxy.useSubTexture) ? Qt.rect(root.sourceVisualRect.x * textureProviderItem.eDPR,
                                                                  root.sourceVisualRect.y * textureProviderItem.eDPR,
                                                                  root.sourceVisualRect.width * textureProviderItem.eDPR,
                                                                  root.sourceVisualRect.height * textureProviderItem.eDPR) : undefined
        }
    }

    // We use texture provider that uses QSGTextureView.
    // QSGTextureView is able to denote a viewport that
    // covers a certain area in the source texture.
    // This way, we don't need to have another layer just
    // to clip the source texture.
    Widgets.TextureProviderItem {
        id: textureProviderItem

        x: effectRect.x
        y: effectRect.y
        width: effectRect.width
        height: effectRect.height

        readonly property Item sourceItem: source?.sourceItem ?? source

        property rect effectRect

        property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

        textureSubRect: Qt.rect(effectRect.x * textureProviderItem.eDPR,
                                effectRect.y * textureProviderItem.eDPR,
                                effectRect.width * textureProviderItem.eDPR,
                                effectRect.height * textureProviderItem.eDPR)

        Connections {
            target: MainCtx

            function onIntfDevicePixelRatioChanged() {
                textureProviderItem.eDPR = MainCtx.effectiveDevicePixelRatio(textureProviderItem.Window.window)
            }
        }

        // Effect's source is sub-texture through the texture provider:
        Binding {
            target: root.effect
            property: root.samplerName
            value: textureProviderItem
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
            value: textureProviderItem
        }
    }
}
