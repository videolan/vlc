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

Label {
    id: label
    color: "white"
    bottomPadding: VLCStyle.margin_xxxsmall
    topPadding: VLCStyle.margin_xxxsmall
    leftPadding: VLCStyle.margin_xxxsmall
    rightPadding: VLCStyle.margin_xxxsmall
    font.pixelSize: VLCStyle.fontSize_normal
    background: Rectangle {
        anchors.fill: label
        color: "black"
        opacity: 0.5
        radius: 3
    }
    Accessible.ignored: true
}
