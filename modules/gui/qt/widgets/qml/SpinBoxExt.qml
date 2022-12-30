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

import org.videolan.vlc 0.1

import "qrc:///style/"

SpinBox {
    id: control
    font.pixelSize: VLCStyle.fontSize_large

    padding: 0
    leftPadding: padding + (control.mirrored ? up.indicator.width : 0)
    rightPadding: padding + (control.mirrored ? 0 : up.indicator.width)

    property color textColor: VLCStyle.colors.buttonText
    property color bgColor: VLCStyle.colors.bg
    property color borderColor:  VLCStyle.colors.buttonBorder
    property int borderWidth: 0

    Keys.priority: Keys.AfterItem
    Keys.onPressed: Navigation.defaultKeyAction(event)

    //ideally we should use Keys.onShortcutOverride but it doesn't
    //work with TextField before 5.13 see QTBUG-68711
    onActiveFocusChanged: {
        if (activeFocus)
            MainCtx.useGlobalShortcuts = false
        else
            MainCtx.useGlobalShortcuts = true
    }

    background: Rectangle {
        radius: VLCStyle.margin_xxxsmall
        border.color: control.borderColor
        border.width: control.borderWidth
        color: control.bgColor
    }

    contentItem: TextInput {
        // NOTE: This is required for InterfaceWindowHandler::applyKeyEvent.
        property bool visualFocus: control.activeFocus

        text: control.textFromValue(control.value, control.locale)

        font: control.font
        color: enabled ? control.textColor : "grey"

        padding: 0
        horizontalAlignment: Qt.AlignRight
        verticalAlignment: Qt.AlignVCenter

        selectByMouse: true
        autoScroll: false
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly

        Keys.priority: Keys.AfterItem

        Keys.onPressed: Navigation.defaultKeyAction(event)
        Keys.onReleased: Navigation.defaultKeyReleaseAction(event)

        Navigation.parentItem: control

    }
    up.indicator: Rectangle {
        x: control.mirrored ? 0: parent.width - width
        height: parent.height / 2
        implicitWidth: VLCStyle.dp(15, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(10, VLCStyle.scale)
        anchors.top: parent.top
        color: control.up.pressed ? VLCStyle.colors.bgHover : control.bgColor
        border.color: control.borderColor

        Text {
            text: "\u2227" // ^ logical AND
            font.pixelSize: control.font.pixelSize * 2
            color: control.textColor
            font.bold: true
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? 0 : parent.width - width
        height: parent.height / 2
        implicitWidth: VLCStyle.dp(15, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(10, VLCStyle.scale)
        anchors.bottom: parent.bottom
        color: control.down.pressed ? VLCStyle.colors.bgHover : control.bgColor
        border.color: control.borderColor

        Text {
            text: "\u2228" // ^ logical OR
            font.pixelSize: control.font.pixelSize * 2
            color: control.textColor
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
