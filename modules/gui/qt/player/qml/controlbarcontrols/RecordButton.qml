/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Widgets.IconToolButton {
    id: control

    enabled: !paintOnly && Player.isPlaying

    color: VLCStyle.colors.record
    text: I18n.qtr("record")

    onClicked: Player.toggleRecord()

    contentItem: T.Label {
        id: content

        anchors.centerIn: parent

        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        text: VLCIcons.record
        color: control.color

        ColorAnimation on color {
            from:  "transparent"
            to: control.color
            //this is an animation and not a transisition, we explicitly want a long duration
            duration: 1000
            loops: Animation.Infinite
            easing.type: Easing.InOutSine
            running: control.enabled && Player.recording

            onStopped: {
                content.color = control.color
            }
        }

        font.pixelSize: control.size
        font.family: VLCIcons.fontFamily
        font.underline: control.font.underline

        Accessible.ignored: true
    }
}
