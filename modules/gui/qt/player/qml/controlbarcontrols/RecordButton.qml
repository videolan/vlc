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
import QtQuick
import QtQuick.Templates as T


import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player

Widgets.IconToolButton {
    id: control

    enabled: !paintOnly && Player.isStarted

    color: "#FFFF0000" //red means recording
    text: VLCIcons.record
    description: qsTr("record")

    onClicked: Player.toggleRecord()

    //IconToolButton already contains a color animation that would conflict
    contentItem: Widgets.IconLabel {
        text: control.text

        color: control.color

        SequentialAnimation on color {
            loops: Animation.Infinite
            running: control.enabled && Player.recording

            ColorAnimation  {
                from:  "#FFFF0000"
                to: "#00FF0000"//use "red" transparent
                //this is an animation and not a transisition, we explicitly want a long duration
                duration: 1000
                easing.type: Easing.InSine
            }

            ColorAnimation  {
                from:"#00FF0000"
                to: "#FFFF0000"
                //this is an animation and not a transisition, we explicitly want a long duration
                duration: 1000
                easing.type: Easing.OutSine
            }

            onStopped: {
                color = "#FFFF0000"
            }
        }
        font: control.font
    }


}
