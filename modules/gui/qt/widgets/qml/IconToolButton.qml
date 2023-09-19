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
import QtQuick.Templates as T

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "."

T.ToolButton {
    id: control

    // Properties

    property bool paintOnly: false

    property string description

    property color color: (control.checked) ? theme.accent : theme.fg.primary

    property color backgroundColor: theme.bg.primary

    // Settings

    padding: 0

    enabled: !paintOnly

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    baselineOffset: contentItem.y + contentItem.baselineOffset

    font.pixelSize: VLCStyle.icon_toolbar
    font.family: VLCIcons.fontFamily

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    // Accessible

    Accessible.onPressAction: control.clicked()

    Accessible.name: description

    // Tooltip

    T.ToolTip.visible: (hovered || visualFocus)

    T.ToolTip.delay: VLCStyle.delayToolTipAppear

    T.ToolTip.text: description

    // Events

    Component.onCompleted: console.assert(text !== "", "text is empty")

    // Children

    readonly property ColorContext colorContext : ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton

        enabled: control.paintOnly || control.enabled
        focused: control.visualFocus
        hovered: control.hovered
        pressed: control.down
    }

    background: AnimatedBackground {
        implicitWidth: control.font.pixelSize
        implicitHeight: control.font.pixelSize

        enabled: theme.initialized

        color: control.backgroundColor

        border.color: visualFocus ? theme.visualFocus : "transparent"
    }

    contentItem: IconLabel {
        text: control.text

        color: control.color

        Behavior on color {
            enabled: theme.initialized
            ColorAnimation {
                duration: VLCStyle.duration_long
            }
        }

        font: control.font
    }
}
