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

import "qrc:///style/"

Row {
    id: windowButtonGroup

    spacing: 0
    padding: 0

    property color color: VLCStyle.colors.text
    property color hoverColor: VLCStyle.colors.windowCSDButtonBg

    CSDWindowButton {
        iconTxt: VLCIcons.window_minimize
        onClicked: topWindow.showMinimized()
        height: windowButtonGroup.height
        color: windowButtonGroup.color
        hoverColor: windowButtonGroup.hoverColor
    }

    CSDWindowButton {
        iconTxt: (topWindow.visibility & Window.Maximized)  ? VLCIcons.window_restore :VLCIcons.window_maximize
        onClicked: {
            if (topWindow.visibility & Window.Maximized) {
                mainInterface.requestInterfaceNormal()
            } else {
                mainInterface.requestInterfaceMaximized()
            }
        }
        height: windowButtonGroup.height
        color: windowButtonGroup.color
        hoverColor: windowButtonGroup.hoverColor
    }

    CSDWindowButton {
        id: closeButton
        iconTxt: VLCIcons.window_close
        onClicked: topWindow.close()
        height: windowButtonGroup.height
        color: closeButton.hovered ? "white" : windowButtonGroup.color
        hoverColor: "red"
    }
}
