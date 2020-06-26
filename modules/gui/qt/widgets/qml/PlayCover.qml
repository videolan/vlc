/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

Rectangle {
    id: root

    property alias iconSize: icon.width
    property bool onlyBorders: false
    signal iconClicked()

    border.color: VLCStyle.colors.accent
    border.width: VLCStyle.table_cover_border

    opacity: visible ? 1 : 0
    gradient: !onlyBorders ? background : undefined
    color: 'transparent'

    Behavior on opacity {
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }

    Image {
        id: icon

        anchors.centerIn: parent
        fillMode: Image.PreserveAspectFit
        source: "qrc:/play_button.svg"
        visible: !root.onlyBorders

        MouseArea {
            anchors.fill: parent
            onClicked: root.iconClicked()
        }
    }

    Gradient {
        id: background

        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, .5) }
        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, .7) }
    }
}
