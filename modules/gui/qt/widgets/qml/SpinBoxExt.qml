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

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

import org.videolan.vlc 0.1

import "qrc:///style/"

T.SpinBox {
    id: control

    property color textColor: theme.fg.primary
    property color bgColor: theme.bg.primary
    property color borderColor:  theme.border
    property int borderWidth: VLCStyle.border

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.SpinBox

        enabled: control.enabled
        focused: (control.visualFocus || textInput.activeFocus)
        hovered: control.hovered
    }

    font.pixelSize: VLCStyle.fontSize_large

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                              contentItem.implicitWidth + 2 * padding +
                              up.implicitIndicatorWidth +
                              down.implicitIndicatorWidth)

    implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding,
                           implicitBackgroundHeight,
                           up.implicitIndicatorHeight,
                           down.implicitIndicatorHeight)

    padding: VLCStyle.margin_xxsmall
    leftPadding: padding + (control.mirrored ? up.indicator.width : 0) + control.borderWidth
    rightPadding: padding + (control.mirrored ? 0 : up.indicator.width) + control.borderWidth
    topPadding: padding + control.borderWidth
    bottomPadding: padding + control.borderWidth

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

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
        id: textInput

        // NOTE: This is required for InterfaceWindowHandler::applyKeyEvent.
        property bool visualFocus: control.activeFocus

        text: control.textFromValue(control.value, control.locale)

        font: control.font
        color: upTheme.fg.primary

        padding: 0
        horizontalAlignment: Qt.AlignRight
        verticalAlignment: Qt.AlignVCenter

        selectByMouse: true
        autoScroll: false
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly

        Keys.priority: Keys.AfterItem

        Navigation.parentItem: control
    }

    up.indicator: Rectangle {
        x: control.mirrored ? control.borderWidth : parent.width - width - control.borderWidth
        height: parent.height / 2
        implicitWidth: VLCStyle.dp(15, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(10, VLCStyle.scale)
        anchors.top: parent.top
        anchors.topMargin: control.borderWidth

        color: upTheme.bg.primary

        ColorContext {
            id: upTheme
            colorSet: ColorContext.ButtonStandard

            enabled: control.enabled
            hovered: control.up.hovered
            pressed: control.up.pressed

            // root control handles focused state
            focused: false
        }

        IconLabel {
            text: VLCIcons.chevron_up
            font.pixelSize: control.font.pixelSize * 2
            color: upTheme.fg.primary
            font.bold: true
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? control.borderWidth : parent.width - width - control.borderWidth
        height: parent.height / 2
        implicitWidth: VLCStyle.dp(15, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(10, VLCStyle.scale)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: control.borderWidth
        color: downTheme.bg.primary

        ColorContext {
            id: downTheme
            colorSet: ColorContext.ButtonStandard

            enabled: control.enabled
            hovered: control.down.hovered
            pressed: control.down.pressed

            // root control handles focused state
            focused: false
        }

        IconLabel {
            text: VLCIcons.chevron_down
            font.pixelSize: control.font.pixelSize * 2
            color: downTheme.fg.primary
            font.bold: true
            anchors.fill: parent
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
