/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

Rectangle {
    id: root

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    property bool active: activeFocus

    // background of this component changes, set it in binding, the changes will be animated
    property color backgroundColor: "transparent"

    // `foregroundColor` property is not used in this component but is
    // provided as a convenience as it gets animated with color property
    property color foregroundColor

    property color activeBorderColor

    property int animationDuration: VLCStyle.duration_long

    property bool animationRunning: borderAnimation.running || bgAnimation.running

    property bool animate: true

    //---------------------------------------------------------------------------------------------
    // Implementation
    //---------------------------------------------------------------------------------------------

    color: backgroundColor

    border.color: root.active
                  ? root.activeBorderColor
                  : VLCStyle.colors.setColorAlpha(root.activeBorderColor, 0)

    border.width: root.active ? VLCStyle.focus_border : 0

    //---------------------------------------------------------------------------------------------
    // Animations
    //---------------------------------------------------------------------------------------------

    Behavior on border.color {
        enabled: root.animate

        ColorAnimation {
            id: borderAnimation

            duration: root.animationDuration
        }
    }

    Behavior on color {
        enabled: root.animate
        ColorAnimation {
            id: bgAnimation

            duration: root.animationDuration
        }
    }

    Behavior on foregroundColor {
        enabled: root.animate
        ColorAnimation {
            duration: root.animationDuration
        }
    }
}
