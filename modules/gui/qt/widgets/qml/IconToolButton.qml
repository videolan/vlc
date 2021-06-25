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

    padding: 0

    property string iconText: ""

    property color color: (control.highlighted) ? VLCStyle.colors.accent
                                                : VLCStyle.colors.icon

    property color colorDisabled: VLCStyle.colors.textInactive
    property color colorOverlay: "transparent"
    property color colorFocus: VLCStyle.colors.bgFocus
    property string textOverlay: ""
    property bool borderEnabled: false
    property bool backgroundVisible: background.active

    enabled: !paintOnly

    ToolTip.text: control.text
    ToolTip.delay: 500

    onActiveFocusChanged: {
        if (!enabled) {
            var keyNavigationLeft = control.KeyNavigation.left
            var keyNavigationRight = control.KeyNavigation.right

            if (!!keyNavigationLeft)
                keyNavigationLeft.forceActiveFocus()
            else if (!!keyNavigationRight)
                keyNavigationRight.forceActiveFocus()
        }
    }

    contentItem: Item {

        Label {
            id: text
            text: control.iconText
            color: (control.enabled) ? background.foregroundColor : control.colorDisabled

            anchors.centerIn: parent

            font.pixelSize: VLCIcons.pixelSize(control.size)
            font.family: VLCIcons.fontFamily
            font.underline: control.font.underline

            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            Accessible.ignored: true

            Label {
                text: control.textOverlay
                color: control.colorOverlay

                anchors.centerIn: parent

                font.pixelSize: VLCIcons.pixelSize(control.size)
                font.family: VLCIcons.fontFamily

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter

                Accessible.ignored: true

            }

            Label {
                text: VLCIcons.active_indicator
                color: (control.enabled) ? background.foregroundColor : control.colorDisabled
                visible: !control.paintOnly && control.checked

                anchors.centerIn: parent

                font.pixelSize: VLCIcons.pixelSize(control.size)
                font.family: VLCIcons.fontFamily

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter

                Accessible.ignored: true
            }

        }
    }

    background: AnimatedBackground {
        id: background

        active: control.activeFocus

        backgroundColor: control.hovered ? VLCStyle.colors.buttonHover
                                         : VLCStyle.colors.setColorAlpha(VLCStyle.colors.buttonHover, 0)

        foregroundColor: control.hovered ? VLCStyle.colors.buttonTextHover
                                         : control.color

        activeBorderColor: control.colorFocus

        implicitHeight: control.size
        implicitWidth : control.size
    }
}
