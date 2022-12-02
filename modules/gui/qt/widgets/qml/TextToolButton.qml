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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

import "qrc:///style/"

T.ToolButton {
    id: control

    font.pixelSize: VLCStyle.fontSize_normal

    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    padding: VLCStyle.margin_xxsmall

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: Navigation.defaultKeyAction(event)

    readonly property ColorContext colorContext : ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton

        enabled: control.enabled
        focused: control.visualFocus
        hovered: control.hovered
        pressed: control.down
    }

    contentItem: T.Label {
        text: control.text
        font: control.font
        color: theme.fg.primary
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    background: AnimatedBackground {
        animate: theme.initialized
        active: visualFocus
        backgroundColor: theme.bg.primary
        activeBorderColor: theme.visualFocus
    }
}
