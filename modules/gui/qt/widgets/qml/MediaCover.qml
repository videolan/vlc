
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

import org.videolan.vlc 0.1
import org.videolan.controls 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


// NOTE: This rectangle is useful to discern the item against a similar background.
// FIXME: Maybe we could refactor this to draw the background directly in the RoundImage.
Rectangle {
    id: root

    // Properties

    property real playIconSize: VLCStyle.play_cover_normal

    property real playCoverBorderWidth: VLCStyle.table_cover_border

    property bool playCoverShowPlay: true

    // Aliases

    property alias source: image.source

    property alias cacheImage: image.cache

    property bool isImageReady: image.status == RoundImage.Ready


    property string fallbackImageSource

    property alias imageOverlay: overlay.sourceComponent

    property alias playCoverVisible: playCoverLoader.visible
    property alias playCoverOpacity: playCoverLoader.opacity

    // Signals

    signal playIconClicked(var /* MouseEvent */ mouse)

    // Settings

    height: VLCStyle.listAlbumCover_height
    width: VLCStyle.listAlbumCover_width

    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Media cover")

    // Children

    RoundImage {
        id: image

        anchors.fill: parent

        radius: root.radius

        cache: false
    }

    RoundImage {
        id: fallbackImage

        anchors.fill: parent

        radius: root.radius

        visible: !root.isImageReady

        // we only keep this image till there is no main image
        // try to release the resources otherwise
        source: !root.isImageReady ? root.fallbackImageSource : ""

        cache: true
    }

    Loader {
        id: overlay

        anchors.fill: parent
    }

    Loader {
        id: playCoverLoader

        anchors.centerIn: parent

        visible: false

        active: false

        sourceComponent: Widgets.PlayCover {
            width: playIconSize

            onClicked: (mouse) => playIconClicked(mouse)
        }

        asynchronous: true

        // NOTE: We are lazy loading the component when this gets visible and it stays loaded.
        //       We could consider unloading it when visible goes to false.
        onVisibleChanged: if (playCoverShowPlay && visible) active = true
    }
}
