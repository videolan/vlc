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
import org.videolan.vlc 0.1
import QtQuick.Window 2.11

import "qrc:///style/"


Window {
    visible: true

    property var rootWindow: g_root
    readonly property point rootLocation: rootWindow.mapToGlobal(rootWindow.x, rootWindow.y)

    width: 300
    minimumWidth: playlistView.minimumWidth
    // minimumHeight: mainInterface.minimumHeight
    title: i18n.qtr("Playlist")
    color: VLCStyle.colors.bg

    Component.onCompleted: {
        minimumHeight = mainInterface.minimumHeight // suppress non-notifyable property binding

        x = rootLocation.x + rootWindow.width + VLCStyle.dp(10, VLCStyle.scale)
        y = rootLocation.y
        height = rootWindow.height
    }

    PlaylistListView {
        id: playlistView
        focus: true
        anchors.fill: parent
    }
}
