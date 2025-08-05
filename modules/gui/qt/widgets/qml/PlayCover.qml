/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

import QtQuick

import VLC.Style
import VLC.Widgets as Widgets

Item {
    id: root

    // Settings

    width: VLCStyle.play_cover_normal
    height: width

    // NOTE: This is the same scaling than the PlayButton, except we make it bigger.
    //       Maybe we could crank this to 1.1.
    scale: (hoverHandler.hovered && !tapHandler.pressed) ? 1.05 : 1.0

    opacity: (visible) ? 1.0 : 0.0

    activeFocusOnTab: false

    signal tapped(var point)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton
    }

    // Animations

    Behavior on scale {
        // NOTE: We disable the animation while pressing to make it more impactful.
        enabled: (!tapHandler.pressed)

        NumberAnimation {
            duration: VLCStyle.duration_short

            easing.type: Easing.OutQuad
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: VLCStyle.duration_short

            easing.type: Easing.OutQuad
        }
    }

    // Children

    TapHandler {
        id: tapHandler

        grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

        onSingleTapped: (eventPoint, button) => {
            root.tapped(point)
        }
    }

    HoverHandler {
        id: hoverHandler

        grabPermissions: PointerHandler.CanTakeOverFromHandlersOfDifferentType | PointerHandler.ApprovesTakeOverByAnything
    }

    Rectangle {
        anchors.fill: parent

        radius: width / 2

        color: "white"

        Widgets.DefaultShadow {

        }
    }

    Widgets.IconLabel {
        anchors.centerIn: parent

        text: VLCIcons.play_filled

        color: (root.hovered) ? theme.accent : "black"

        font.pixelSize: Math.round(parent.width / 2)
    }
}
