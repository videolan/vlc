
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

import org.videolan.vlc 0.1
import "qrc:///style/"

Rectangle {
    id: root

    property int orientation: Qt.Vertical
    property int margin: VLCStyle.margin_xxxsmall

    readonly property ColorContext colorContext: ColorContext {
        id: theme
    }

    property Item source: parent

    property int length: 0

    property var _position: [
        {
            // for orientation == Qt.Vertical
            "width" : VLCStyle.heightBar_xxxsmall,
            "height": root.length,
            "x": margin,
            "y": !!source ? (source.height - root.length) / 2 : 0
        },
        {
            // for orientation == Qt.Horizontal
            "width": root.length,
            "height": VLCStyle.heightBar_xxxsmall,
            "x": !!source ? (source.width - root.length) / 2 : 0,
            "y": !!source ? source.height - margin : 0,
        }
    ]

    property var _currentPosition: (orientation === Qt.Vertical) ? _position[0] : _position[1]

    color: theme.accent

    x: _currentPosition.x
    y: _currentPosition.y
    width: _currentPosition.width
    height: _currentPosition.height
}
