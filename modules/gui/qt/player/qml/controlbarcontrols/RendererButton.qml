/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player

Widgets.IconToolButton {
    id: root

    signal requestLockUnlockAutoHide(bool lock)

    text: VLCIcons.renderer
    description: qsTr("Renderer")

    // NOTE: We want to pop the menu above the button.
    onClicked: menu.popup(this.mapToGlobal(0, 0), true)

    QmlRendererMenu {
        id: menu

        ctx: MainCtx

        onAboutToShow: root.requestLockUnlockAutoHide(true)
        onAboutToHide: root.requestLockUnlockAutoHide(false)
    }

    function forceUnlock() {
        if(menu)
            menu.close()
    }
}
