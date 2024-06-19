/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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


import VLC.Style

Control {
    id: root

    padding: VLCStyle.margin_xxxsmall
    font.pixelSize: VLCStyle.fontSize_normal

    hoverEnabled: false

    required property string text

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Badge
    }

    background: Rectangle {
        border.color: theme.border
        radius: VLCStyle.dp(6, VLCStyle.scale)
        color: theme.bg.primary
        opacity: 0.8
    }

    contentItem: Row {
        spacing: VLCStyle.margin_xxxsmall

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font: root.font

            color: theme.fg.primary

            visible: (text.length > 0)
        }

        BusyIndicator {
            palette.dark: theme.fg.primary

            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
