
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
import QtGraphicalEffects 1.0

import "qrc:///style/"

Item {
    id: root

    property alias source: primaryShadow.source
    property alias primaryVerticalOffset: primaryShadow.verticalOffset
    property alias primaryRadius: primaryShadow.radius
    property alias primarySamples: primaryShadow.samples
    property alias secondaryVerticalOffset: secondaryShadow.verticalOffset
    property alias secondaryRadius: secondaryShadow.radius
    property alias secondarySamples: secondaryShadow.samples

    DropShadow {
        id: primaryShadow

        anchors.fill: parent
        horizontalOffset: 0
        spread: 0
        color: Qt.rgba(0, 0, 0, .22)
        samples: 1 + radius * 2
    }

    DropShadow {
        id: secondaryShadow

        anchors.fill: parent
        source: primaryShadow.source
        horizontalOffset: 0
        spread: 0
        color: Qt.rgba(0, 0, 0, .18)
        samples: 1 + radius * 2
    }
}
