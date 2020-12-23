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
    id: root

    property bool active: activeFocus
    property bool selected: false
    property color defaultForeground: VLCStyle.colors.text
    property color foregroundColor: VLCStyle.colors.text

    states: [
        State {
            name: "selected"

            PropertyChanges {
                target: root
                color: VLCStyle.colors.bgHoverInactive
                foregroundColor: VLCStyle.colors.bgHoverTextInactive
            }
        },
        State {
            name: "active"

            PropertyChanges {
                target: root
                color: VLCStyle.colors.accent
                foregroundColor: VLCStyle.colors.accentText
            }
        },
        State {
            name: "normal"

            PropertyChanges {
                target: root
                color: "transparent"
                foregroundColor: root.defaultForeground
            }
        }
    ]

    transitions: Transition {
        to: "*"

        ColorAnimation {
            property: "color"
            duration: 100
            easing.type: Easing.InOutSine
        }
    }

    state: {
        if (active || activeFocus)
            return "active"
        if (selected)
            return "selected"
        return "normal"
    }
}
