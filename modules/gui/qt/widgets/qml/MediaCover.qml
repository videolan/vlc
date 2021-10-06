
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

// NOTE: This rectangle is useful to discern the item against a similar background.
// FIXME: Maybe we could refactor this to draw the background directly in the RoundImage.
Rectangle {
    id: root

    // Properties

    property bool playCoverShowGradient: true

    property real playIconSize: VLCStyle.play_cover_normal

    property real playCoverBorderWidth: VLCStyle.table_cover_border

    // Aliases

    property alias source: image.source

    property alias imageOverlay: overlay.sourceComponent

    property alias playCoverOpacity: playCoverLoader.opacity
    property alias playCoverVisible: playCoverLoader.visible

    // Signals

    signal playIconClicked

    // Settings

    height: VLCStyle.listAlbumCover_height
    width: VLCStyle.listAlbumCover_width

    color: VLCStyle.colors.grid

    // Children

    RoundImage {
        id: image

        anchors.fill: parent

        radius: root.radius
    }

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
            showGradient: playCoverShowGradient
            iconSize: playIconSize
            radius: root.radius

            onIconClicked: playIconClicked()
        }

        onVisibleChanged: {
            if (visible && !active)
                active = true
        }
    }
}
