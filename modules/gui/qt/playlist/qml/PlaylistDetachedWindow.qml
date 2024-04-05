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

import QtQuick
import QtQuick.Window

import org.videolan.vlc 0.1

import "qrc:///style/"


Window {
    visible: MainCtx.playlistVisible

    transientParent: MainCtx.intfMainWindow

    property alias playlistView: playlistView

    width: 350
    minimumWidth: playlistView.minimumWidth

    title: qsTr("Playlist")
    color: theme.bg.primary

    onVisibleChanged: {
        if (visible) {
            console.assert(transientParent)
            height = transientParent.height
            minimumHeight = transientParent.minimumHeight
            x = transientParent.x + transientParent.width + 10
            y = transientParent.y
        }
    }

    onClosing: {
        MainCtx.playlistVisible = false
    }

    PlaylistListView {
        id: playlistView

        useAcrylic: false
        focus: true
        anchors.fill: parent

        colorContext.palette: VLCStyle.palette
    }
}
