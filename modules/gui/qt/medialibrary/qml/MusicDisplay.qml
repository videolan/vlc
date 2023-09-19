/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Controls
import QtQuick.Layouts

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Widgets.PageLoader {
    id: root

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Music view")

    pageModel: [{
            displayText: qsTr("Artists"),
            name: "artists",
            default: true,
            url: "qrc:///medialibrary/MusicArtistsDisplay.qml"
        }, {
            displayText: qsTr("Albums"),
            name: "albums",
            url: "qrc:///medialibrary/MusicAlbumsDisplay.qml"
        }, {
            displayText: qsTr("Tracks"),
            name: "tracks" ,
            url: "qrc:///medialibrary/MusicTracksDisplay.qml"
        }, {
            displayText: qsTr("Genres"),
            name: "genres" ,
            url: "qrc:///medialibrary/MusicGenresDisplay.qml"
        }, {
            displayText: qsTr("Playlists"),
            name: "playlists" ,
            url: "qrc:///medialibrary/MusicPlaylistsDisplay.qml"
        }
    ]

    function loadIndex(index) {
        const pageName = root.pageModel[index].name
        if (root.isDefaulLoadedForPath([pageName]))
            return
        History.push([...root.pagePrefix, pageName])
    }

    property ListModel tabModel: ListModel {
        Component.onCompleted: {
            pageModel.forEach(function(e) {
                append({
                           displayText: e.displayText,
                           name: e.name,
                       })
            })
        }
    }

    localMenuDelegate: Widgets.LocalTabBar {
        currentView: root.pageName
        model: tabModel

        onClicked: (index) => {
            root.loadIndex(index)
        }
    }
}
