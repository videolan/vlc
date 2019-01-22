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
import QtQuick.Controls 2.4

import "qrc:///style/"

Slider {
    id: control
    anchors.margins: VLCStyle.margin_xxsmall

    value: player.position
    onMoved: player.position = control.position

    height: control.barHeight + VLCStyle.fontHeight_normal + VLCStyle.margin_xxsmall * 2
    implicitHeight: control.barHeight + VLCStyle.fontHeight_normal + VLCStyle.margin_xxsmall * 2

    topPadding: 0
    leftPadding: 0
    bottomPadding: 0
    rightPadding: 0

    stepSize: 0.01

    property int barHeight: 5

    background: Rectangle {
        width: control.availableWidth
        implicitHeight: control.implicitHeight
        height: implicitHeight
        color: "transparent"

        Rectangle {
            width: control.visualPosition * parent.width
            height: control.barHeight
            color: control.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.bgHover
            radius: control.barHeight
        }

        Text {
            text: player.time.toString()
            color: VLCStyle.colors.text
            font.pixelSize: VLCStyle.fontSize_normal
            anchors {
                bottom: parent.bottom
                bottomMargin: VLCStyle.margin_xxsmall
                left: parent.left
                leftMargin: VLCStyle.margin_xxsmall
            }
        }

        Text {
            text: player.length.toString()
            color: VLCStyle.colors.text
            font.pixelSize: VLCStyle.fontSize_normal
            anchors {
                bottom: parent.bottom
                bottomMargin: VLCStyle.margin_xxsmall
                right: parent.right
                rightMargin: VLCStyle.margin_xxsmall
            }
        }
    }

    handle: Rectangle {
        visible: control.activeFocus
        x: (control.visualPosition * control.availableWidth) - width / 2
        y: (control.barHeight - width) / 2
        implicitWidth: VLCStyle.margin_small
        implicitHeight: VLCStyle.margin_small
        radius: VLCStyle.margin_small
        color: VLCStyle.colors.accent
    }
}
