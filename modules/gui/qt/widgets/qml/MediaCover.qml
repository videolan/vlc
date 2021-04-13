
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

Widgets.RoundImage {
    id: root

    property alias playCoverOpacity: playCover.opacity
    property alias playCoverVisible: playCover.visible
    property alias playCoverOnlyBorders: playCover.onlyBorders
    property alias playIconSize: playCover.iconSize
    property alias playCoverBorder: playCover.border
    property alias imageOverlay: overlay.sourceComponent
    signal playIconClicked

    height: VLCStyle.listAlbumCover_height
    width: VLCStyle.listAlbumCover_width

    Loader {
        id: overlay

        anchors.fill: parent
    }

    Widgets.PlayCover {
        id: playCover

        anchors.fill: parent
        iconSize: VLCStyle.play_root_small
        radius: root.radius

        onIconClicked: root.playIconClicked()
    }
}
