/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQuick.Layouts
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers


Control {
    padding: VLCStyle.focus_border

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    property bool paintOnly: false

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        border.color: visualFocus ? theme.visualFocus : "transparent"
    }

    contentItem: Widgets.MenuLabel {
        text: qsTr("WIDGET\nNOT\nFOUND")
        horizontalAlignment: Text.AlignHCenter
        color: theme.fg.primary
    }
}
