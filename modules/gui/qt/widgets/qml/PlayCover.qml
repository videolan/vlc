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
import QtQuick.Templates as T

import VLC.Style
import VLC.Widgets as Widgets

T.RoundButton {
    id: root

    // Settings

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    width: VLCStyle.play_cover_normal
    height: width

    // NOTE: This is the same scaling than the PlayButton, except we make it bigger.
    //       Maybe we could crank this to 1.1.
    scale: (hovered && !pressed) ? 1.05 : 1.0

    property real _hoverProgress: hovered ? 1.0 : 0.0

    activeFocusOnTab: false

    signal tapped(var point)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton
    }

    onClicked: {
        tapped(Qt.point(pressX, pressY))
    }

    // Animations

    Behavior on scale {
        // NOTE: We disable the animation while pressing to make it more impactful.
        enabled: (!root.pressed)

        NumberAnimation {
            duration: VLCStyle.duration_short

            easing.type: Easing.OutQuad
        }
    }

    Behavior on _hoverProgress {
        NumberAnimation {
            duration: VLCStyle.duration_short

            easing.type: Easing.InOutSine
        }
    }

    // Children

    background: Rectangle {
        radius: root.radius

        color: "white"

        Widgets.DefaultShadow {

        }
    }

    contentItem: Widgets.IconLabel {
        text: VLCIcons.play_filled

        color: {
            const accent = theme.accent
            const t = root._hoverProgress
            return Qt.rgba(accent.r * t, accent.g * t, accent.b * t, 1)
        }

        font.pixelSize: Math.round(parent.width / 2)
    }
}
