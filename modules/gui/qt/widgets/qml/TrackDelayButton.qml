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

import QtQuick
import QtQuick.Layouts

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

ButtonExt {
    id: control

    //Signals

    signal animate()

    extBackgroundAnimation: blinkAnimation.running

    onAnimate: blinkAnimation.start()

    property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ButtonStandard

        focused: control.visualFocus
        hovered: control.hovered
        enabled: control.enabled
        pressed: control.down
    }

    SequentialAnimation {
        id: blinkAnimation
        loops: 2

        ColorAnimation {
            target: control.background
            property: "backgroundColor"
            from: theme.bg.primary
            to: theme.accent
            duration: VLCStyle.duration_long
        }
        ColorAnimation {
            target: control.background
            property: "backgroundColor"
            from: theme.accent
            to: theme.bg.primary
            duration: VLCStyle.duration_long
        }
    }
}
