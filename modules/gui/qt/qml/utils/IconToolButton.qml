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

ToolButton {
    id: control
    property color color: control.checked
                        ? (control.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.bgHover )
                        : VLCStyle.colors.buttonText
    property int size: VLCStyle.icon_normal

    property color highlightColor: control.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.bgHover

    contentItem: Label {
        text: control.text
        color: control.color

        font.pixelSize: control.size
        font.family: VLCIcons.fontFamily

        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        anchors {
            centerIn: parent
            //verticalCenter: parent.verticalCenter
            //rightMargin: VLCStyle.margin_xsmall
            //leftMargin: VLCStyle.margin_small
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 2
            visible: control.activeFocus || control.checked
            color: control.highlightColor
        }
    }

    background: Rectangle {
        implicitHeight: control.size
        implicitWidth: control.size
        color: "transparent"
    }
}
