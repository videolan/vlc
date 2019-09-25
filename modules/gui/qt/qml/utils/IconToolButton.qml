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
    property bool paintOnly: false

    property int size: VLCStyle.icon_normal

    property color highlightColor: VLCStyle.colors.accent

    padding: 0

    property color color: VLCStyle.colors.buttonText
    property color colorDisabled: VLCStyle.colors.lightText
    property color colorOverlay: "transparent"
    property string textOverlay: ""
    property bool borderEnabled: false

    enabled: !paintOnly

    contentItem: Item {

        Rectangle {
            anchors.fill: parent
            visible: control.activeFocus || control.hovered || control.highlighted
            color: highlightColor
        }

        Label {
            id: text
            text: control.text
            color: control.enabled ? control.color : control.colorDisabled

            anchors.centerIn: parent

            font.pixelSize: control.size
            font.family: VLCIcons.fontFamily
            font.underline: control.font.underline

            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            Label {
                text: control.textOverlay
                color: control.colorOverlay

                anchors.centerIn: parent

                font.pixelSize: control.size
                font.family: VLCIcons.fontFamily

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter

            }

            Label {
                text: VLCIcons.active_indicator
                color: control.enabled ? control.color : control.colorDisabled
                visible: !control.paintOnly && control.checked

                anchors.centerIn: parent

                font.pixelSize: control.size
                font.family: VLCIcons.fontFamily

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }

        }
    }

    background: Rectangle {
        implicitHeight: control.size
        implicitWidth: control.size
        color: "transparent"
    }
}
