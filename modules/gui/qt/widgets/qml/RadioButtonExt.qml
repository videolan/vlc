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

// Based on Qt Quick Basic Style:
T.RadioButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_xxsmall
    spacing: VLCStyle.margin_xxsmall

    Keys.priority: Keys.AfterItem
    Keys.onPressed: function(event) {
        Navigation.defaultKeyAction(event)
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ButtonStandard

        focused: control.visualFocus
        hovered: control.hovered
        enabled: control.enabled
        pressed: control.down
    }

    // keep in sync with RadioDelegate.qml (shared RadioIndicator.qml was removed for performance reasons)
    indicator: Rectangle {
        implicitWidth: control.contentItem ? implicitHeight : VLCStyle.dp(28, VLCStyle.scale)
        implicitHeight: control.contentItem ? control.contentItem.implicitHeight : VLCStyle.dp(28, VLCStyle.scale)

        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: width / 2
        color: control.down ? Qt.lighter(theme.accent) : theme.bg.primary
        border.width: control.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : VLCStyle.dp(1, VLCStyle.scale)
        border.color: control.visualFocus ? theme.accent : theme.fg.primary

        Rectangle {
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            width: parent.width * 3 / 4
            height: parent.height * 3 / 4
            radius: width / 2
            color: theme.accent
            visible: control.checked
        }
    }

    contentItem: ListLabel {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        color: theme.fg.primary
    }
}
