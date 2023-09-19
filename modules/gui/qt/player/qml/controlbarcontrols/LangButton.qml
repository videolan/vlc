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

import QtQuick

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///player/" as Player

Widgets.IconToolButton {
    id: root

    // Proprerties

    readonly property var _parentItem: {
        if ((typeof rootPlayer !== 'undefined') && (rootPlayer !== null))
            return rootPlayer
        else
            return g_mainDisplay
    }

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    signal menuOpened(var menu)

    // Settings

    text: VLCIcons.audiosub

    enabled: menuLoader.status === Loader.Ready

    description: qsTr("Languages and tracks")

    // Events

    onClicked: menuLoader.item.open()

    // Children

    Loader {
        id: menuLoader

        sourceComponent: Player.TracksMenu {
            id: menu

            parent: root._parentItem

            x: 0
            y: (parent.positionSliderY - height)
            z: 1

            focus: true

            colorContext.palette: root.colorContext.palette

            onOpened: {
                root.requestLockUnlockAutoHide(true)

                root.menuOpened(menu)
            }

            onClosed: {
                root.requestLockUnlockAutoHide(false)
                root.forceActiveFocus()

                root.menuOpened(null)
            }
        }
    }
}
