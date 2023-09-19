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

//CSD is only supported on Qt 5.15
import QtQuick
import QtQuick.Window

import org.videolan.vlc 0.1

Item {
    TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onSingleTapped: {
            if (eventPoint.event.button & Qt.RightButton) {
                const systemButton = MainCtx.csdButtonModel.systemMenuButton
                if (systemButton) {
                    systemButton.showSystemMenu(eventPoint.position)
                }
            }
        }

        onDoubleTapped: {
            if (!(eventPoint.event.button & Qt.LeftButton))
                return

            // handle left button click
            if ((MainCtx.intfMainWindow.visibility & Window.Maximized) !== 0) {
                MainCtx.requestInterfaceNormal()
            } else {
                MainCtx.requestInterfaceMaximized()
            }

        }
        gesturePolicy: TapHandler.DragThreshold
    }

    HoverHandler {
        // explicitely set cursor shape here so no other control can interfere, causing confusion (see #28115)
        cursorShape: Qt.ArrowCursor
    }

    DragHandler {
        target: null
        grabPermissions: TapHandler.CanTakeOverFromAnything
        onActiveChanged: {
            if (active) {
                MainCtx.intfMainWindow.startSystemMove();
            }
        }
    }

}
