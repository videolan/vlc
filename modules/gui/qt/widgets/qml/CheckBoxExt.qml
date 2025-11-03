/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 * Copyright (C) 2017 The Qt Company Ltd.
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

import VLC.MainInterface
import VLC.Style
import VLC.Widgets

// Based on Qt Quick Basic Style:
T.CheckBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_xxsmall
    spacing: VLCStyle.margin_xxsmall

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ButtonStandard

        focused: control.visualFocus
        hovered: control.hovered
        enabled: control.enabled
        pressed: control.down
    }

    // keep in sync with CheckDelegate.qml (shared CheckIndicator.qml was removed for performance reasons)
    indicator: Rectangle {
        implicitWidth: control.contentItem ? implicitHeight : VLCStyle.dp(28, VLCStyle.scale)
        implicitHeight: control.contentItem ? control.contentItem.implicitHeight : VLCStyle.dp(28, VLCStyle.scale)

        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        color: control.down ? Qt.lighter(theme.accent) : theme.bg.primary
        border.width: control.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : VLCStyle.dp(1, VLCStyle.scale)
        border.color: control.visualFocus ? theme.accent : theme.fg.primary

        Rectangle {
            anchors.centerIn: parent
            anchors.alignWhenCentered: false

            width: Math.min(parent.width, parent.height) / 1.618 // golden ratio
            height: width

            color: theme.accent

            visible: (control.checkState === Qt.PartiallyChecked)
        }

        Text {
            anchors.fill: parent

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            font.pixelSize: Math.min(width, height)
            font.weight: Font.DemiBold
            minimumPixelSize: 1
            fontSizeMode: Text.Fit

            color: theme.accent

            text: "✓"

            visible: (control.checkState === Qt.Checked)
        }
    }

    contentItem: ListLabel {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        color: theme.fg.primary
    }
}
