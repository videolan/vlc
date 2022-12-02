/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.ImageToolButton {
    id: control

    text: I18n.qtr("A to B")

    sourceSize.width: VLCStyle.icon_toolbar
    sourceSize.height: VLCStyle.icon_toolbar

    checked: Player.ABloopState !== Player.ABLOOP_STATE_NONE
    onClicked: Player.toggleABloopState()

    //imageSource: "qrc:///icons/atob.svg"
    imageSource: {
        switch(Player.ABloopState) {
        case Player.ABLOOP_STATE_A:
            return control._colorize(
                control.colorContext.accent,
                control.colorContext.fg.primary
            )
        case Player.ABLOOP_STATE_B:
            return control._colorize(
                control.colorContext.accent,
                control.colorContext.accent
            )
        case Player.ABLOOP_STATE_NONE:
        default:
            return control._colorize(
                control.colorContext.fg.primary,
                control.colorContext.fg.primary
            )
        }
    }

    function _colorize(a, b) {
        return SVGColorImage.colorize("qrc:///icons/atob.svg")
            .color1(control.colorContext.fg.primary)
            .any({
                "#AAAAAA": a,
                "#BBBBBB": b,
            }).uri()
    }
}
