/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

import QtQuick

// A convenience file to encapsulate two drop shadow rectangles stacked
Item {
    id: root

    property alias radius: primaryShadow.radius

    property alias primaryColor: primaryShadow.color
    property alias primaryVerticalOffset: primaryShadow.yOffset
    property alias primaryHorizontalOffset: primaryShadow.xOffset
    property alias primaryBlurRadius: primaryShadow.blurRadius

    property alias secondaryColor: secondaryShadow.color
    property alias secondaryVerticalOffset: secondaryShadow.yOffset
    property alias secondaryHorizontalOffset: secondaryShadow.xOffset
    property alias secondaryBlurRadius: secondaryShadow.blurRadius

    primaryColor: Qt.rgba(0, 0, 0, .18)
    secondaryColor: Qt.rgba(0, 0, 0, .22)

    RoundedRectangleShadow {
        id: primaryShadow

        parent: root.parent // similar to how Repeater behaves
        z: root.z

        opacity: root.opacity
        visible: root.visible
        enabled: root.enabled
    }

    RoundedRectangleShadow {
        id: secondaryShadow

        parent: root.parent // similar to how Repeater behaves
        z: root.z

        opacity: root.opacity
        visible: root.visible
        enabled: root.enabled

        radius: root.radius
    }
}
