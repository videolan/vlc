
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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

import org.videolan.controls 0.1

RoundImage {
    id: root

    property alias playCoverOpacity: playCoverLoader.opacity
    property alias playCoverVisible: playCoverLoader.visible
    property bool playCoverOnlyBorders: false
    property real playIconSize: VLCStyle.play_cover_normal
    property real playCoverBorderWidth: VLCStyle.table_cover_border
    property alias imageOverlay: overlay.sourceComponent
    signal playIconClicked

    height: VLCStyle.listAlbumCover_height
    width: VLCStyle.listAlbumCover_width

    Loader {
        id: overlay

        anchors.fill: parent
    }

    Loader {
        id: playCoverLoader

        anchors.fill: parent
        visible: false
        active: false
        sourceComponent: Widgets.PlayCover {
            onlyBorders: root.playCoverOnlyBorders
            iconSize: root.playIconSize
            border.width: root.playCoverBorderWidth
            radius: root.radius

            onIconClicked: root.playIconClicked()
        }

        onVisibleChanged: {
            if (visible && !active)
                active = true
        }
    }
}
