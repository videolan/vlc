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

ToolButton {
    id: control

    property bool paintOnly: false

    property int size: VLCStyle.icon_normal

    property string iconText: ""

    // background colors
    property color backgroundColor: "transparent"
    property color backgroundColorHover: VLCStyle.colors.buttonHover

    // foreground colors based on state
    property color color: VLCStyle.colors.icon
    property color colorHover: VLCStyle.colors.buttonTextHover
    property color colorHighlighted: VLCStyle.colors.accent
    property color colorDisabled: VLCStyle.colors.textInactive

    // active border color
    property color colorFocus: VLCStyle.colors.bgFocus

    enabled: !paintOnly

    padding: 0

    ToolTip.text: control.text
    ToolTip.delay: 500

    contentItem: Label {
        id: text

        text: control.iconText
        color: background.foregroundColor

        anchors.centerIn: parent

        font.pixelSize: VLCIcons.pixelSize(control.size)
        font.family: VLCIcons.fontFamily
        font.underline: control.font.underline

        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        Accessible.ignored: true

        Label {
            text: VLCIcons.active_indicator
            color: background.foregroundColor
            visible: !control.paintOnly && control.checked

            anchors.centerIn: parent

            font.pixelSize: VLCIcons.pixelSize(control.size)
            font.family: VLCIcons.fontFamily

            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            Accessible.ignored: true
        }
    }

    background: AnimatedBackground {
        id: background

        active: control.activeFocus

        backgroundColor: {
            if (control.hovered)
                return control.backgroundColorHover
            if (control.backgroundColor.a === 0) // if base color is transparent, animation starts with black color
                return VLCStyle.colors.setColorAlpha(control.backgroundColorHover, 0)
            return control.backgroundColor
        }

        foregroundColor: {
            if (control.hovered)
                return control.colorHover
            if (control.highlighted)
                return control.colorHighlighted
            if (!control.enabled)
                return control.colorDisabled
            return control.color
        }

        activeBorderColor: control.colorFocus

        implicitHeight: control.size
        implicitWidth : control.size
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: Navigation.defaultKeyAction(event)
}
