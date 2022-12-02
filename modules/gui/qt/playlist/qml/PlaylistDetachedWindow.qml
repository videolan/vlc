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


Window {
    visible: true

    // TODO: Qt >5.13 use transientParent
    property QtWindow parentWindow: MainCtx.intfMainWindow

    width: 350
    minimumWidth: playlistView.minimumWidth

    title: I18n.qtr("Playlist")
    color: theme.bg.primary

    Loader {
        asynchronous: true
        source: "qrc:///menus/GlobalShortcuts.qml"
    }

    Component.onCompleted: {
        if (!!parentWindow) {
            height = parentWindow.height
            minimumHeight = parentWindow.minimumHeight

            x = parentWindow.x + parentWindow.width + 10
            y = parentWindow.y
        } else {
            height = 400
            minimumHeight = 200
        }
    }

    PlaylistListView {
        id: playlistView

        useAcrylic: false
        focus: true
        anchors.fill: parent

        colorContext.palette: VLCStyle.palette

        readonly property PlaylistListView g_root: playlistView
    }
}
