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

import QtQuick 2.11
import QtQuick.Controls 2.4

import org.videolan.vlc 0.1
import "qrc:///style/"
import "qrc:///widgets/" as Widgets

MouseArea {
    // Settings

    width: VLCStyle.play_cover_normal
    height: width

    // NOTE: This is the same scaling than the PlayButton, except we make it bigger.
    //       Maybe we could crank this to 1.1.
    scale: (containsMouse && pressed === false) ? 1.05 : 1.0

    opacity: (visible) ? 1.0 : 0.0

    hoverEnabled: true

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton
    }

    // Animations

    Behavior on scale {
        // NOTE: We disable the animation while pressing to make it more impactful.
        enabled: (pressed === false)

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

    Image {
        anchors.centerIn: parent

        // NOTE: We round this to avoid blurry textures with the QML renderer.
        width: Math.round(parent.width * 3.2)
        height: width

        z: -1

        source: VLCStyle.playShadow
    }

    Rectangle {
        anchors.fill: parent

        radius: width

        color: "white"
    }

    Widgets.IconLabel {
        anchors.centerIn: parent

        text: VLCIcons.play

        color: (containsMouse) ? theme.accent
                               : "black"

        font.pixelSize: Math.round(parent.width / 2)
    }
}
