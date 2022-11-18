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
import "qrc:///style/"

//we want the progress bar to match the radius of the of the video thumbnail
//so we generarte two rectangles with the right radius and we clip the part we
//want to hide
Item {
    id: progressBar

    implicitHeight: VLCStyle.dp(4, VLCStyle.scale)

    clip :true

    property real value: 0
    property int radius: implicitHeight

    Rectangle {

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        height: Math.max(progressBar.radius * 2, //to have at least the proper radius applied
                         parent.height + radius) //the top radius section should be always clipped

        color: "white"
        radius: progressBar.radius

        //use clipping again to delimit the viewed part as we want the accent section to follow the given as well
        Item {
            clip: true

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: progressBar.value * parent.width

            Rectangle {
                width: progressBar.width

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left

                color: VLCStyle.colors.accent
                radius: progressBar.radius
            }
        }
    }
}
