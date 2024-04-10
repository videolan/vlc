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
import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Item {
    id: root

    property var networkModel
    property color bgColor
    property color color1
    property color accent


    // Image properties
    property int fillMode: Image.Stretch
    property int horizontalAlignment: Image.AlignHCenter
    property int verticalAlignment: Image.AlignVCenter

    readonly property var paintedWidth: _image.paintedWidth
    readonly property var paintedHeight: _image.paintedHeight

    // currently shown image
    property var _image: typeImage.visible ? typeImage : artwork


    function _baseUri(type) {
        switch (type) {
        case NetworkMediaModel.TYPE_DISC:
            return "qrc:///sd/disc.svg"
        case NetworkMediaModel.TYPE_CARD:
            return "qrc:///sd/capture-card.svg"
        case NetworkMediaModel.TYPE_STREAM:
            return "qrc:///sd/stream.svg"
        case NetworkMediaModel.TYPE_PLAYLIST:
            return "qrc:///sd/playlist.svg"
        case NetworkMediaModel.TYPE_FILE:
            return "qrc:///sd/file.svg"
        default:
            return "qrc:///sd/directory.svg"
        }
    }


    Widgets.ScaledImage {
        // failsafe cover, we show this while loading artwork or if loading fails

        id: typeImage

        anchors.fill: parent

        visible: !artwork.visible

        sourceSize: Qt.size(width, height)

        fillMode: root.fillMode
        horizontalAlignment: root.horizontalAlignment
        verticalAlignment: root.verticalAlignment

        source: {
            if (!networkModel || !visible)
                return ""

            const img = SVGColorImage.colorize(_baseUri(networkModel.type))
                .color1(root.color1)
                .accent(root.accent)

            if (bgColor !== undefined)
                img.background(root.bgColor)

            return img.uri()
        }
    }

    Widgets.ScaledImage {
        id: artwork

        anchors.fill: parent

        visible: status === Image.Ready

        sourceSize: Qt.size(width, height)

        fillMode: root.fillMode
        horizontalAlignment: root.horizontalAlignment
        verticalAlignment: root.verticalAlignment

        source: {
            if (networkModel?.artwork && networkModel.artwork.length > 0)
                return VLCAccessImage.uri(networkModel.artwork)

            return ""
        }
    }
}
