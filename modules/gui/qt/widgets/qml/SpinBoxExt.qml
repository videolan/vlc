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

SpinBox{
    id: control
    font.pixelSize: VLCStyle.fontSize_large

    property color textColor: VLCStyle.colors.buttonText
    property color bgColor: VLCStyle.colors.bg
    property color borderColor:  VLCStyle.colors.buttonBorder

    background: Rectangle {
        implicitWidth: VLCStyle.dp(4)
        implicitHeight: VLCStyle.dp(32)
        border.color: control.borderColor
        color: control.bgColor
    }

    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)

        font: control.font
        color: enabled ? control.textColor : "grey"

        horizontalAlignment: Qt.AlignRight
        verticalAlignment: Qt.AlignVCenter
        selectByMouse: true
        autoScroll: false
        readOnly: !control.editable
        validator: control.validator
    }
    up.indicator: Rectangle {
        x: parent.width - width
        height: parent.height / 2
        implicitWidth: VLCStyle.dp(15)
        implicitHeight: VLCStyle.dp(10)
        anchors.top: parent.top
        color: control.up.pressed ? VLCStyle.colors.bgHover : control.bgColor
        border.color: control.borderColor

        Text {
            text: "+"
            font.pixelSize: control.font.pixelSize * 2
            color: control.textColor
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    down.indicator: Rectangle {
        x: parent.width - width
        height: parent.height / 2
        implicitWidth: VLCStyle.dp(15)
        implicitHeight: VLCStyle.dp(10)
        anchors.bottom: parent.bottom
        color: control.down.pressed ? VLCStyle.colors.bgHover : control.bgColor
        border.color: control.borderColor

        Text {
            text: "-"
            font.pixelSize: control.font.pixelSize * 2
            color: control.textColor
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
