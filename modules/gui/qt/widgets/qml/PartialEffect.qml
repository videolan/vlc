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

    property Item effect
    property string samplerName: "source"

    // Enable blending if background of source is not opaque.
    // This comes with a performance penalty.
    property alias blending: sourceProxy.blending

    // This item displays the item with the rect denoted by effectRect
    // being transparent (only when blending is set):
    ShaderEffect {
        id: sourceProxy

        anchors.fill: parent

        blending: false

        property alias source: root.source

        readonly property rect discardRect: {
            if (blending)
                return Qt.rect(textureProviderItem.x / root.width,
                               textureProviderItem.y / root.height,
                               (textureProviderItem.x + textureProviderItem.width) / root.width,
                               (textureProviderItem.y + textureProviderItem.height) / root.height)
            else // If blending is not enabled, no need to make the normalization calculations
                return Qt.rect(0, 0, 0, 0)
        }

        // cullMode: ShaderEffect.BackFaceCulling // QTBUG-136611 (Layering breaks culling with OpenGL)

        // Simple filter that is only enabled when blending is active.
        // We do not want the source to be rendered below the frosted glass effect.
        // NOTE: It might be a better idea to enable this at all times if texture sampling
        //       is costlier than branching.

        fragmentShader: blending ? "qrc:///shaders/RectFilter.frag.qsb" : ""
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

        onDprChanged: {
            eDPR = MainCtx.effectiveDevicePixelRatio(Window.window)
        }

        onChildrenChanged: {
            // Do not add visual(QQuickItem) children to this item,
            // because Qt thinks that it needs to use implicit layering.
            // "If needed, MultiEffect will internally generate a
            // ShaderEffectSource as the texture source."
            // Adding children to a texture provider item is not going
            // make them rendered in the texture. Instead, simply add
            // to the source. If source is layered, the source would
            // be a `ShaderEffectSource`, in that case `sourceItem`
            // can be used to reach to the real source.
            console.assert(textureProviderItem.children.length === 0)
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
