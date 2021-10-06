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

    // Properties

    property bool showGradient: true

    // Aliases

    property alias iconSize: icon.width

    // Signals

    signal iconClicked()

    // Settings

    opacity: (visible)

    color: (showGradient) ? undefined : "transparent"

    gradient: (showGradient) ? background : undefined

    // Animations

    Behavior on opacity {
        NumberAnimation { duration: VLCStyle.duration_fast; easing.type: Easing.OutQuad }
    }

    // Children

    Gradient {
        id: background

        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.5) }
        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.7) }
    }

    Image {
        id: icon

        anchors.centerIn: parent

        visible: showGradient

        source: "qrc:/play_button.svg"

        fillMode: Image.PreserveAspectFit

        mipmap: (width < VLCStyle.icon_normal)

        MouseArea {
            anchors.fill: parent

            onClicked: iconClicked()
        }
    }
}
