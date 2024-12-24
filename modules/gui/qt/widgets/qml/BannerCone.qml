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

import QtQuick


import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

Image {
    id: root

    required property color color
    property alias csdMenuVisible: csdMenu.menuVisible

    sourceSize: Qt.size(VLCStyle.icon_normal, VLCStyle.icon_normal)

    mipmap: MainCtx.useXmasCone()
    source: MainCtx.useXmasCone() ? "qrc:///logo/vlc48-xmas.png" // TODO: new xmas cone for designs?
                                  : SVGColorImage.colorize("qrc:///misc/cone.svg").accent(root.color).uri()

    focus: false

    TapHandler {
        enabled:  MainCtx.clientSideDecoration

        gesturePolicy: TapHandler.WithinBounds

        onSingleTapped: (eventPoint, button) => {
            if (button === Qt.LeftButton){
                doubleTapFilter.start()
            }
        }

        onDoubleTapped: (eventPoint, button) => {
            if (button === Qt.LeftButton) {
                doubleTapFilter.stop()
                MainCtx.intfMainWindow.close()
            }
        }
    }

    CSDMenu {
        id: csdMenu
        ctx: MainCtx
    }

    //SingleTapped is always notified, perform the action only if the double tap doesn't occur
    Timer {
        id: doubleTapFilter

        interval: VLCStyle.duration_short

        onTriggered: {
            //popup below the widget
            csdMenu.popup(root.mapToGlobal(0, root.height + VLCStyle.margin_xxsmall))
        }
    }
}
