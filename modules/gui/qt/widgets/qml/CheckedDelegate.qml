
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
import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

import VLC.Widgets as Widgets
import VLC.Style

T.ItemDelegate {
    id: control

    // Settings

    leftPadding: VLCStyle.margin_xlarge
    rightPadding: VLCStyle.margin_xsmall

    checkable: true

    font.pixelSize: VLCStyle.fontSize_large

    // Childs

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        focused: control.activeFocus
        hovered: control.hovered
        pressed: control.pressed
        enabled: control.enabled
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        color: control.checked ? theme.bg.highlight : theme.bg.primary
        border.color: visualFocus ? theme.visualFocus : "transparent"
    }

    contentItem: Item { // don't use a row, it will move text when control is unchecked
        Widgets.IconLabel {
            id: checkIcon

            height: parent.height

            verticalAlignment: Text.AlignVCenter

            visible: control.checked

            text: VLCIcons.check

            color: theme.fg.primary

            font.pixelSize: VLCStyle.icon_track
        }

        Widgets.MenuLabel {
            id: text

            anchors.left: checkIcon.right

            height: parent.height
            width: parent.width - checkIcon.width

            leftPadding: VLCStyle.margin_normal

            verticalAlignment: Text.AlignVCenter

            text: control.text
            font: control.font

            color: theme.fg.primary
        }
    }
}
