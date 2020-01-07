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

import QtQuick 2.0

import "qrc:///style/"

Rectangle {
    property bool active: activeFocus
    property bool selected: false

    onActiveChanged: {
        animateSelected.running = false
        if (active) {
            animateActive.restart()
        } else {
            if (selected)
                color = VLCStyle.colors.bgHoverInactive
            else
                color = "transparent"
            animateActive.running = false
        }
    }

    onSelectedChanged: {
        if (active)
            return
        color = "transparent"
        if (selected) {
            animateSelected.restart()
        } else {
            animateSelected.running = false
        }

    }

    color: "transparent"
    ColorAnimation on color {
        id: animateActive
        running: false
        to: VLCStyle.colors.accent
        duration: 200
        easing.type: Easing.OutCubic
    }
    ColorAnimation on color {
        id: animateSelected
        running: false
        to: VLCStyle.colors.bgHoverInactive
        duration: 200
        easing.type: Easing.OutCubic
    }
}
