/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

Image {
    id: root

    property var button: MainCtx.csdButtonModel.systemMenuButton

    sourceSize.width: VLCStyle.icon_normal
    sourceSize.height: VLCStyle.icon_normal
    source: SVGColorImage.colorize("qrc:///misc/cone.svg")
                    .accent(VLCStyle.colors.accent)
                    .uri()

    focus: false

    Loader {
        anchors.fill: root
        enabled: MainCtx.clientSideDecoration && root.button

        sourceComponent: MouseArea {
            onClicked: { root.button.click() }
            onDoubleClicked: { root.button.doubleClick() }

            Connections {
                // don't target MouseArea for position since we
                // need position updates of cone, inresepect of BannerSources
                // to correctly track cone's global position
                target: root

                // handles VLCStyle.scale changes
                onXChanged: Qt.callLater(root.updateRect)
                onYChanged: Qt.callLater(root.updateRect)
                onWidthChanged: Qt.callLater(root.updateRect)
                onHeightChanged: Qt.callLater(root.updateRect)
            }

            Connections {
                target: VLCStyle

                // handle window resize
                onAppWidthChanged: Qt.callLater(root.updateRect)
                onAppHeightChanged: Qt.callLater(root.updateRect)
            }
        }
    }

    function updateRect() {
        var rect = root.mapToItem(null, 0, 0, width, height)

        if (button)
            button.rect = rect
    }
}
