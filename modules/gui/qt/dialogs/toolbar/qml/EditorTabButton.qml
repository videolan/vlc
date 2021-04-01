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
import "qrc:///widgets/" as Widgets

TabButton {
    id: mainPlayerControl

    property int index: 0
    property bool active: index == bar.currentIndex

    implicitWidth: VLCStyle.button_width_large

    contentItem: Widgets.ListLabel {
        text: mainPlayerControl.text
        horizontalAlignment: Text.AlignHCenter
    }

    background: Rectangle {
        color: active ? VLCStyle.colors.bgAlt : hovered ? VLCStyle.colors.bgHover : VLCStyle.colors.bg

        border.color: VLCStyle.colors.accent
        border.width: active ? VLCStyle.dp(1, VLCStyle.scale) : 0

        Rectangle {
            width: parent.width - parent.border.width * 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.bottom

            anchors.topMargin: -(height / 2)

            color: parent.color

            visible: active

            height: parent.border.width * 2
        }
    }
}
