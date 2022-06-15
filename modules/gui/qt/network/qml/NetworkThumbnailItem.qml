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

    readonly property string artworkSource: !!rowModel ? rowModel.artwork : ""

    readonly property bool _showPlayCover: (currentlyFocused || containsMouse)
                                           && !!rowModel
                                           && (rowModel.type !== NetworkMediaModel.TYPE_NODE)
                                           && (rowModel.type !== NetworkMediaModel.TYPE_DIRECTORY)

    readonly property bool _showCustomCover: (!artworkSource) || (artwork.status != Image.Ready)

    signal playClicked(int index)

    Widgets.ListCoverShadow {
        anchors.fill: !item._showCustomCover ? artwork : background
        source: !item._showCustomCover ? artwork : background
    }

    Rectangle {
        id: background

        anchors.verticalCenter: parent.verticalCenter
        color: VLCStyle.colors.bg
        width: VLCStyle.listAlbumCover_width
        height: VLCStyle.listAlbumCover_height
        radius: VLCStyle.listAlbumCover_radius
        visible: item._showCustomCover

        NetworkCustomCover {
            networkModel: rowModel
            anchors.fill: parent
            iconSize: VLCStyle.icon_small
        }

        Widgets.PlayCover {
            anchors.centerIn: parent

            width: VLCStyle.play_cover_small

            visible: item._showPlayCover

            onClicked: playClicked(item.index)
        }
    }

    Widgets.ScaledImage {
        id: artwork

        x: Math.round((width - paintedWidth) / 2)
        y: Math.round((parent.height - paintedHeight) / 2)
        width: VLCStyle.listAlbumCover_width
        height: VLCStyle.listAlbumCover_height
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop
        source: item.artworkSource
        visible: !item._showCustomCover

        Widgets.PlayCover {
            x: Math.round((artwork.paintedWidth - width) / 2)
            y: Math.round((artwork.paintedHeight - height) / 2)

            width: VLCStyle.play_cover_small

            visible: item._showPlayCover

            onClicked: playClicked(item.index)
        }
    }
}
