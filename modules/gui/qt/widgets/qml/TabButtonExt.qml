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
import QtQuick.Layouts 1.3

import "qrc:///style/"

TabButton {
    id: control
    text: model.displayText

    padding: 0
    width: contentItem.implicitWidth

    property string iconTxt: ""
    property string bgColor: "transparent"
    property bool selected: false

    font.pixelSize: VLCStyle.fontSize_normal

    background: Rectangle {
        height: parent.height
        width: parent.contentItem.width
        color: control.bgColor
    }

    contentItem: Item {
        implicitWidth: tabRow.width
        implicitHeight: tabRow.height

        Rectangle {
            anchors.fill: tabRow
            visible: control.activeFocus || control.hovered
            color: VLCStyle.colors.accent
        }

        Row {
            id: tabRow
            padding: VLCStyle.margin_xxsmall
            spacing: VLCStyle.margin_xxsmall

            Label {
                id: icon
                anchors.verticalCenter: parent.verticalCenter
                color: VLCStyle.colors.buttonText

                font.pixelSize: VLCStyle.icon_topbar
                font.family: VLCIcons.fontFamily
                horizontalAlignment: Text.AlignHCenter
                rightPadding: VLCStyle.margin_xsmall

                visible: control.iconTxt !== ""
                text: control.iconTxt
            }

            Label {
                text: control.text
                font: control.font
                color: VLCStyle.colors.text
                padding: VLCStyle.margin_xxsmall

                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            anchors {
                left: tabRow.left
                right: tabRow.right
                bottom: tabRow.bottom
            }
            height: 2
            visible: control.selected
            color: "transparent"
            border.color: VLCStyle.colors.accent
        }
    }
}
