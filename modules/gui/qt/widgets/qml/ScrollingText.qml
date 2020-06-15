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

Item {
    id: control

    clip: true

    property Text label: undefined
    property bool scroll: false

    readonly property bool _needsToScroll: (label.width < label.contentWidth)

    SequentialAnimation {
        id: scrollAnimation

        running: control.scroll && control._needsToScroll
        loops: Animation.Infinite

        onStopped: {
            label.x = 0
        }

        PauseAnimation {
            duration: 1000
        }

        SmoothedAnimation {
            target: label
            property: "x"
            from: 0
            to: label.width - label.contentWidth

            maximumEasingTime: 0
            velocity: 20
        }

        PauseAnimation {
            duration: 1000
        }

        PropertyAction {
            target: label
            property: "x"
            value: 0
        }
    }

}

