
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
import QtQuick.Templates 2.4 as T

import "qrc:///style/"
import "qrc:///util/KeyHelper.js" as KeyHelper

T.ItemDelegate {
    id: control

    checkable: true
    leftPadding: VLCStyle.margin_xlarge
    rightPadding: VLCStyle.margin_xsmall

    background: Rectangle {
        color: control.hovered || control.activeFocus ? "#2A2A2A" : "transparent"
    }

    contentItem: Item { // don't use a row, it will move text when control is unchecked
        IconLabel {
            id: checkIcon

            text: VLCIcons.check
            visible: control.checked
            height: parent.height
            font.pixelSize: 24
            color: "white"
            verticalAlignment: Text.AlignVCenter
        }

        MenuLabel {
            id: text

            anchors.left: checkIcon.right
            height: parent.height
            font: control.font
            text: control.text
            color: "white"
            verticalAlignment: Text.AlignVCenter
            leftPadding: VLCStyle.margin_normal
            width: parent.width - checkIcon.width
        }
    }
}
