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

import "qrc:///style/"

ToolButton {
    id: control
    property bool paintOnly: false

    property int size: VLCStyle.icon_normal

    padding: 0

    property string iconText: ""
    property alias color: backgroundHover.foregroundColor
    property color colorDisabled: VLCStyle.colors.textInactive
    property color colorOverlay: "transparent"
    property string textOverlay: ""
    property bool borderEnabled: false
    property bool backgroundVisible: backgroundHover.active

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
            color: control.enabled ? backgroundHover.foregroundColor : control.colorDisabled

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
                color: control.enabled ? backgroundHover.foregroundColor : control.colorDisabled
                visible: !control.paintOnly && control.checked

                anchors.centerIn: parent

                font.pixelSize: VLCIcons.pixelSize(control.size)
                font.family: VLCIcons.fontFamily

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter

                Accessible.ignored: true
            }

        }

        BackgroundFocus {
            anchors.fill: parent

            visible: control.activeFocus
        }
    }

    background: BackgroundHover {
        id: backgroundHover

        active: control.hovered

        foregroundColor: (control.highlighted) ? VLCStyle.colors.accent
                                               : VLCStyle.colors.icon

        implicitHeight: control.size
        implicitWidth : control.size
    }
}
