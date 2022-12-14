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
import QtQuick 2.11
import QtQuick.Window 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"

Row {
    id: windowButtonGroup

    spacing: 0
    padding: 0

    width: implicitWidth

    property color color: VLCStyle.colors.text
    property color hoverColor: VLCStyle.colors.windowCSDButtonBg

    readonly property bool hovered: {
        var h = false
        for (var i = 0; i < repeater.count; ++i) {
            var button = repeater.itemAt(i)
            h = h || button.hovered
        }

        return h
    }

    Repeater {
        id: repeater

        model: MainCtx.csdButtonModel.windowCSDButtons

        CSDWindowButton {
            height: windowButtonGroup.height

            color: (modelData.type === CSDButton.Close && (hovered)) ? "white" : windowButtonGroup.color

            hoverColor: (modelData.type === CSDButton.Close) ? "red" : windowButtonGroup.hoverColor

            iconTxt: {
                switch (modelData.type) {
                case CSDButton.Minimize:
                    return VLCIcons.window_minimize

                case CSDButton.MaximizeRestore:
                    return (MainCtx.intfMainWindow.visibility === Window.Maximized)
                            ? VLCIcons.window_restore
                            : VLCIcons.window_maximize

                case CSDButton.Close:
                    return VLCIcons.window_close
                }

                console.assert(false, "unreachable")
            }

            onClicked: modelData.click()
        }
    }
}
