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

    property color color: theme.fg.primary
    property color hoverColor: VLCStyle.colors.setColorAlpha(theme.bg.primary, 0.5)

    readonly property bool hovered: {
        var h = false
        for (var i = 0; i < repeater.count; ++i) {
            var button = repeater.itemAt(i)
            h = h || button.hovered || button.showHovered
        }

        return h
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Repeater {
        id: repeater

        model: MainCtx.csdButtonModel.windowCSDButtons

        CSDWindowButton {
            height: windowButtonGroup.height

            showHovered: modelData.showHovered

            color: (modelData.type === CSDButton.Close && (hovered || modelData.showHovered))
                   ? "white"
                   : windowButtonGroup.color

            hoverColor: (modelData.type === CSDButton.Close) ? "red" : windowButtonGroup.hoverColor

            isThemeDark: theme.palette.isDark

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

            // handles VLCStyle.scale changes
            onWidthChanged: Qt.callLater(updateRect)
            onHeightChanged: Qt.callLater(updateRect)

            Connections {
                target: VLCStyle.self

                // handle window resize
                onAppWidthChanged: Qt.callLater(updateRect)
                onAppHeightChanged: Qt.callLater(updateRect)
            }

            function updateRect() {
                var point = mapToItem(null, 0, 0)
                var rect = Qt.rect(point.x, point.y, width, height)

                modelData.rect = rect
            }
        }
    }
}
