/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Rectangle {
    id: root
    property var artist: ({})

    color: VLCStyle.colors.bg

    height: VLCStyle.heightBar_xlarge

    Rectangle {
        id: artistImageContainer
        color: VLCStyle.colors.banner

        height: VLCStyle.cover_small
        width: VLCStyle.cover_small

        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left
            leftMargin: VLCStyle.margin_small

        }

        Image {
            id: artistImage
            source: artist.cover || VLCStyle.noArtArtist
            fillMode: Image.PreserveAspectCrop
            anchors.fill: parent
        }

        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: VLCStyle.cover_small
                height: VLCStyle.cover_small
                radius: VLCStyle.cover_small
            }
        }
    }

    Text {
        id: artistName
        text: artist.name || qsTr("No artist")

        anchors {
            verticalCenter: parent.verticalCenter
            left: artistImageContainer.right
            right: parent.right
            leftMargin: VLCStyle.margin_small
            rightMargin: VLCStyle.margin_small
        }

        font.pixelSize: VLCStyle.fontSize_xxlarge
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        color: VLCStyle.colors.text
    }
}
