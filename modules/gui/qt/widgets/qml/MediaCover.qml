
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


import VLC.Widgets as Widgets
import VLC.Style


// NOTE: This rectangle is useful to discern the item against a similar background.
// FIXME: Maybe we could refactor this to draw the background directly in the RoundImage.
Rectangle {
    id: root

    // Properties

    property real playIconSize: VLCStyle.play_cover_normal

    property bool playCoverShowPlay: true

    // Aliases

    property alias source: image.source

    property alias cacheImage: image.cache

    property bool isImageReady: image.status == Widgets.RoundImage.Ready


    property string fallbackImageSource

    property alias imageOverlay: overlay.sourceComponent

    property alias playCoverVisible: playCoverLoader.visible
    property alias playCoverOpacity: playCoverLoader.opacity

    required property int pictureWidth
    required property int pictureHeight

    // Signals

    signal playIconClicked(var point)

    // Settings

    height: VLCStyle.listAlbumCover_height
    width: VLCStyle.listAlbumCover_width

    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Media cover")

    // Children

    //delay placeholder showing up
    Timer {
        id: timer
        //changing running value will start/stop the timer,
        //the running value will change back to false when the timer
        //timeout regardless of the binding
        running: image.state === Image.Loading
        interval: VLCStyle.duration_long
    }

    Widgets.RoundImage {
        id: image

        anchors.fill: parent

        radius: root.radius

        sourceSize.width: root.pictureWidth
        sourceSize.height: root.pictureHeight

        cache: false
    }

    Widgets.RoundImage {
        id: fallbackImage

        anchors.fill: parent

        radius: root.radius

        visible: !root.isImageReady && !timer.running

        // we only keep this image till there is no main image
        // try to release the resources otherwise
        source: !root.isImageReady ? root.fallbackImageSource : ""

        sourceSize.width: root.pictureWidth
        sourceSize.height: root.pictureHeight

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

            Component.onCompleted: {
                tapped.connect(root.playIconClicked)
            }
        }

        asynchronous: true

        // NOTE: We are lazy loading the component when this gets visible and it stays loaded.
        //       We could consider unloading it when visible goes to false.
        onVisibleChanged: if (playCoverShowPlay && visible) active = true
    }
}
