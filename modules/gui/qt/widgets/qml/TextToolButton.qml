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

    contentItem: T.Label {
        text: control.text
        font: control.font
        color: VLCStyle.colors.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        anchors {
            verticalCenter: parent.verticalCenter
            rightMargin: VLCStyle.margin_xsmall
            leftMargin: VLCStyle.margin_small
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 2
            visible: control.checked
            color: control.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.bgHover
        }
    }

    background: AnimatedBackground {
        active: visualFocus

        backgroundColor: control.hovered ? VLCStyle.colors.buttonHover
                                         : VLCStyle.colors.setColorAlpha(VLCStyle.colors.buttonHover, 0)
    }
}
