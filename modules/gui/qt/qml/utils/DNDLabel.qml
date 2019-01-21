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
import "qrc:///style/"

Rectangle {
    property alias text: label.text

    z: 1
    width:  label.implicitWidth
    height: label.implicitHeight
    color: VLCStyle.colors.button
    border.color : VLCStyle.colors.buttonBorder
    visible: false

    Drag.active: visible

    function updatePos(x, y) {
        var pos = root.mapFromGlobal(x, y)
        dragItem.x = pos.x + 10
        dragItem.y = pos.y + 10
    }

    Text {
        id: label
        font.pixelSize: VLCStyle.fontSize_normal
        color: VLCStyle.colors.text
        text: qsTr("%1 tracks selected").arg(delegateModel.selectedGroup.count)
    }
}
