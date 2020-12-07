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
import QtGraphicalEffects 1.0

import "qrc:///style/"

Item {
    id: root

    property real radius: 3
    property alias asynchronous: cover.asynchronous
    property alias fillMode: cover.fillMode
    property alias mipmap: cover.mipmap
    property alias paintedHeight: cover.paintedHeight
    property alias paintedWidth: cover.paintedWidth
    property alias source: cover.source
    property alias sourceSize: cover.sourceSize
    property alias status: cover.status
    property alias horizontalAlignment: cover.horizontalAlignment
    property alias verticalAlignment: cover.verticalAlignment

    Image {
        id: cover

        anchors.fill: parent
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(width, height)
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                radius: root.radius
                width: root.width
                height: root.height
                visible: false
            }
        }
    }
}
