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

TabButton {
    id: mainPlayerControl

    property int index: 0
    property bool active: index == bar.currentIndex

    contentItem: Text {
        text: mainPlayerControl.text
        color: VLCStyle.colors.buttonText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        width: VLCStyle.button_width_large
        height: VLCStyle.heightBar_normal
        color: active ? VLCStyle.colors.bgAlt : hovered ? VLCStyle.colors.bgHover : VLCStyle.colors.bg
        radius: 2
    }
}
