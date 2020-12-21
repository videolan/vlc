
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

    property int orientation: Qt.Vertical
    property int margin: VLCStyle.margin_xxxsmall

    color: VLCStyle.colors.accent
    width: orientation === Qt.Vertical ? VLCStyle.heightBar_xxxsmall : parent.width
    height: orientation === Qt.Horizontal ? VLCStyle.heightBar_xxxsmall : parent.height

    onOrientationChanged: {
        if (orientation == Qt.Vertical) {
            anchors.horizontalCenter = undefined
            anchors.verticalCenter = Qt.binding(function () {
                return parent.verticalCenter
            })
            anchors.left = Qt.binding(function () {
                return parent.left
            })
            anchors.right = undefined
            anchors.leftMargin = Qt.binding(function () {
                return margin
            })
            anchors.bottomMargin = 0
        } else {
            anchors.top = undefined
            anchors.bottom = Qt.binding(function () {
                return parent.bottom
            })
            anchors.horizontalCenter = Qt.binding(function () {
                return parent.horizontalCenter
            })
            anchors.verticalCenter = undefined
            anchors.leftMargin = 0
            anchors.bottomMargin = Qt.binding(function () {
                return margin
            })
        }
    }
}
