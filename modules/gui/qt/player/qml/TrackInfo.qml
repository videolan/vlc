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
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Item {
    id: root

    RowLayout {
        id: rowLayout
        anchors.fill: parent

        Image {
            //color:"red"
            Layout.preferredWidth: VLCStyle.heightAlbumCover_small
            Layout.preferredHeight: VLCStyle.heightAlbumCover_small
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            fillMode    : Image.PreserveAspectFit
            source: (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString()) ? mainPlaylistController.currentItem.artwork : VLCStyle.noArtCover
        }

        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                font.pixelSize: VLCStyle.fontSize_normal
                color: VLCStyle.colors.text
                font.bold: true

                text: mainPlaylistController.currentItem.title
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                font.pixelSize: VLCStyle.fontSize_small

                color: VLCStyle.colors.text
                text: mainPlaylistController.currentItem.artist
            }

        }

    }

}
