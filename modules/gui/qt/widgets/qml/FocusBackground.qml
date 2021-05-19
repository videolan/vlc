/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    property bool active: activeFocus

    property color foregroundColor: foregroundColorBase

    //---------------------------------------------------------------------------------------------
    // Style

    property int durationAnimation: 100

    property color backgroundColor: VLCStyle.colors.buttonHover

    property color foregroundColorBase  : VLCStyle.colors.text
    property color foregroundColorActive: VLCStyle.colors.buttonTextHover

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    // NOTE: We want the set the proper transparent color to avoid animating from black.
    color: VLCStyle.colors.setColorAlpha(backgroundColor, 0)

    states: State {
        name: "active"

        when: (active || activeFocus)

        PropertyChanges {
            target: root

            color: backgroundColor

            foregroundColor: foregroundColorActive
        }
    }

    transitions: Transition {
        ColorAnimation {
            property: "color"

            duration: durationAnimation

            easing.type: Easing.InOutSine
        }
    }
}
