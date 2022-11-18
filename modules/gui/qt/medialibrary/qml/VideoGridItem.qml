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

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

Widgets.GridItem {
    id: root

    property alias showNewIndicator: image.visible
    
    property var labels: [
        model.resolution_name || "",
        model.channel || ""
    ].filter(function(a) { return a !== "" })

    function play() {
        if ( model.id !== undefined ) {
            g_mainDisplay.showPlayer()
            MediaLib.addAndPlay( model.id )
        }
    }

    image: model.thumbnail || VLCStyle.noArtVideoCover
    title: model.title || I18n.qtr("Unknown title")
    subtitle: model.duration.formatHMS() || ""
    pictureWidth: VLCStyle.gridCover_video_width
    pictureHeight: VLCStyle.gridCover_video_height
    playCoverBorderWidth: VLCStyle.gridCover_video_border
    titleMargin: VLCStyle.margin_xxsmall

    pictureOverlay: Item {
        width: root.pictureWidth
        height: root.pictureHeight

        Widgets.VideoQualityLabels {
            anchors {
                top: parent.top
                right: parent.right
                topMargin: VLCStyle.margin_xxsmall
                leftMargin: VLCStyle.margin_xxsmall
                rightMargin: VLCStyle.margin_xxsmall
            }

            labels: root.labels
        }

        Widgets.VideoProgressBar {
            id: progressBar

            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }

            visible: (model.progress > 0)

            radius: root.pictureRadius
            value: Helpers.clamp(model.progress, 0, 1)
        }
    }

    onPlayClicked: root.play()

    Image {
        id: image

        anchors.right: parent.right
        anchors.top: parent.top

        width: VLCStyle.gridItem_newIndicator
        height: width

        visible: false

        source: VLCStyle.newIndicator
    }
}
