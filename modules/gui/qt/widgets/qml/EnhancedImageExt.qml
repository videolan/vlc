/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

import VLC.Widgets
import VLC.Util

ImageExt {
    id: root

    textureProviderItem: textureProvider

    // NOTE: Unlike `sourceClipRect`, `textureSubRect` acts as viewport for the texture,
    //       thus faster. No manipulations are done either to the image or the texture.
    //       Prefer using `textureSubRect` if the rectangle is not static, and prefer using
    //       `sourceClipRect` otherwise to save system and video  memory. As a reminder,
    //       implicit size reflects the texture size.
    // WARNING: Using this property may be incompatible with certain filling modes.
    property alias textureSubRect: textureProvider.textureSubRect

    property alias textureProvider: textureProvider

    // NOTE: Target is by default the texture provider `ImageExt` provides, but it can be
    //       set to any texture provider. For example, `ShaderEffectSource` can be displayed
    //       rounded this way.
    property alias targetTextureProvider: textureProvider.source
    targetTextureProvider: sourceTextureProviderItem

    // No need to load images in this case:
    loadImages: (targetTextureProvider === root.sourceTextureProviderItem)

    TextureProviderItem {
        id: textureProvider

        // `Image` interface, as `ImageExt` needs it:
        readonly property int status: (source instanceof Image ? ((source.status === Image.Ready && observer.isValid) ? Image.Ready : Image.Loading)
                                                               : (observer.isValid ? Image.Ready : Image.Null))

        implicitWidth: (source instanceof Image) ? source.implicitWidth : textureSize.width
        implicitHeight: (source instanceof Image) ? source.implicitHeight : textureSize.height

        readonly property bool sourceNeedsTiling: (root.fillMode === Image.Tile ||
                                                   root.fillMode === Image.TileVertically ||
                                                   root.fillMode === Image.TileHorizontally)

        detachAtlasTextures: sourceNeedsTiling

        horizontalWrapMode: sourceNeedsTiling ? TextureProviderItem.Repeat : TextureProviderItem.ClampToEdge
        verticalWrapMode: sourceNeedsTiling ? TextureProviderItem.Repeat : TextureProviderItem.ClampToEdge

        textureSubRect: sourceNeedsTiling ? Qt.rect(0, 0, root.paintedWidth, root.paintedHeight) : undefined

        property size textureSize

        Connections {
            target: root.Window.window
            enabled: root.visible && textureProvider.source && !(textureProvider.source instanceof Image)

            function onAfterAnimating() {
                textureProvider.textureSize = observer.textureSize
            }
        }

        TextureProviderObserver {
            id: observer
            source: textureProvider
        }
    }
}
