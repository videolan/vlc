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
import QtQuick.Controls 2.4
import QtQml.Models 2.2
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Item {
    id: item

    property var rowModel: parent.rowModel
    property var model: parent.colModel
    readonly property bool currentlyFocused: parent.currentlyFocused
    readonly property bool containsMouse: parent.containsMouse
    readonly property int index: parent.index

    signal playClicked(var index)

    Rectangle {
        id: background

        color: VLCStyle.colors.bg
        width: VLCStyle.listAlbumCover_width
        height: VLCStyle.listAlbumCover_height
        visible: !artwork.visible

        Image {
            id: custom_cover

            anchors.centerIn: parent
            sourceSize.height: VLCStyle.icon_small
            sourceSize.width: VLCStyle.icon_small
            fillMode: Image.PreserveAspectFit
            mipmap: true
            source: {
                switch (rowModel.type) {
                case NetworkMediaModel.TYPE_DISC:
                    return "qrc:///type/disc.svg"
                case NetworkMediaModel.TYPE_CARD:
                    return "qrc:///type/capture-card.svg"
                case NetworkMediaModel.TYPE_STREAM:
                    return "qrc:///type/stream.svg"
                case NetworkMediaModel.TYPE_PLAYLIST:
                    return "qrc:///type/playlist.svg"
                case NetworkMediaModel.TYPE_FILE:
                    return "qrc:///type/file_black.svg"
                default:
                    return "qrc:///type/directory_black.svg"
                }
            }
        }

        ColorOverlay {
            anchors.fill: custom_cover
            source: custom_cover
            color: VLCStyle.colors.text
            visible: rowModel.type !== NetworkMediaModel.TYPE_DISC
                     && rowModel.type !== NetworkMediaModel.TYPE_CARD
                     && rowModel.type !== NetworkMediaModel.TYPE_STREAM
        }
    }

    Image {
        id: artwork

        x: (width - paintedWidth) / 2
        y: (height - paintedHeight) / 2
        width: VLCStyle.listAlbumCover_width
        height: VLCStyle.listAlbumCover_height
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop
        source: item.rowModel.artwork
        visible: item.rowModel.artwork
                 && item.rowModel.artwork.toString() !== ""
        mipmap: true
    }

    Widgets.PlayCover {
        x: artwork.visible ? artwork.x : background.x
        y: artwork.visible ? artwork.y : background.y
        width: artwork.visible ? artwork.paintedWidth : background.width
        height: artwork.visible ? artwork.paintedHeight : background.height
        iconSize: VLCStyle.play_cover_small
        visible: currentlyFocused || containsMouse
        onIconClicked: playClicked(item.index)
        onlyBorders: rowModel.type === NetworkMediaModel.TYPE_NODE
                     || rowModel.type === NetworkMediaModel.TYPE_DIRECTORY
    }
}
