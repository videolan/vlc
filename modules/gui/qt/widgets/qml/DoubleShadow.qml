/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

// A convenience file to encapsulate two drop shadow images stacked on top
// of each other
Item {
    id: root

    property var xRadius: null
    property var yRadius: null

    property alias primaryVerticalOffset: primaryShadow.yOffset
    property alias primaryHorizontalOffset: primaryShadow.xOffset
    property alias primaryColor: primaryShadow.color
    property alias primaryBlurRadius: primaryShadow.blurRadius
    property alias primaryXRadius: primaryShadow.xRadius
    property alias primaryYRadius: primaryShadow.yRadius

    property alias secondaryVerticalOffset: secondaryShadow.yOffset
    property alias secondaryHorizontalOffset: secondaryShadow.xOffset
    property alias secondaryColor: secondaryShadow.color
    property alias secondaryBlurRadius: secondaryShadow.blurRadius
    property alias secondaryXRadius: secondaryShadow.xRadius
    property alias secondaryYRadius: secondaryShadow.yRadius

    property alias cache: primaryShadow.cache

    visible: (width > 0 && height > 0)

    DropShadowImage {
        id: primaryShadow

        anchors.centerIn: parent

        color: Qt.rgba(0, 0, 0, .18)
        xOffset: 0

        xRadius: root.xRadius
        yRadius: root.yRadius

        sourceSize: Qt.size(parent.width, parent.height)
    }

    DropShadowImage {
        id: secondaryShadow

        anchors.centerIn: parent

        color: Qt.rgba(0, 0, 0, .22)
        xOffset: 0

        xRadius: root.xRadius
        yRadius: root.yRadius

        sourceSize: Qt.size(parent.width, parent.height)

        cache: root.cache
    }
}
