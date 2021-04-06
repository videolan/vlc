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

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

Widgets.GridItem {
    id: root

    property bool showNewIndicator: true
    property int newIndicatorMedian: root.highlighted ? VLCStyle.icon_small : VLCStyle.icon_xsmall

    image: model.thumbnail || VLCStyle.noArtCover
    title: model.title || i18n.qtr("Unknown title")
    subtitle: Helpers.msToString(model.duration) || ""
    labels: [
        model.resolution_name || "",
        model.channel || ""
    ].filter(function(a) { return a !== "" } )
    pictureWidth: VLCStyle.gridCover_video_width
    pictureHeight: VLCStyle.gridCover_video_height
    playCoverBorder.width: VLCStyle.gridCover_video_border
    titleMargin: VLCStyle.margin_xxsmall

    pictureOverlay: Item {
        width: root.pictureWidth
        height: root.pictureHeight

        Widgets.VideoProgressBar {
            id: progressBar

            visible: !root.highlighted && value > 0
            value: model.progress > 0 ? model.progress : 0
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                rightMargin: root.pictureRadius
            }
        }
    }

    onPlayClicked: {
        if ( model.id !== undefined ) {
            g_mainDisplay.showPlayer()
            medialib.addAndPlay( model.id )
        }
    }
    
    Behavior on newIndicatorMedian {
        NumberAnimation {
            duration: 200
            easing.type: Easing.InOutSine
        }
    }

    Item {
        clip: true
        x: parent.width - width
        y: 0
        width: 2 * root.newIndicatorMedian
        height: 2 * root.newIndicatorMedian
        visible: root.showNewIndicator && model.progress <= 0

        Rectangle {
            x: parent.width - root.newIndicatorMedian
            y: - root.newIndicatorMedian
            width: 2 * root.newIndicatorMedian
            height: 2 * root.newIndicatorMedian
            color: VLCStyle.colors.accent
            rotation: 45
        }
    }
}
