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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

import org.videolan.medialib 0.1

Widgets.PageLoader {
    id: root

    property var sortModel
    property var contentModel
    property bool isViewMultiView: true

    defaultPage: "artists"
    pageModel: [{
            displayText: i18n.qtr("Artists"),
            name: "artists",
            url: "qrc:///medialibrary/MusicArtistsDisplay.qml"
        }, {
            displayText: i18n.qtr("Albums"),
            name: "albums",
            url: "qrc:///medialibrary/MusicAlbumsDisplay.qml"
        }, {
            displayText: i18n.qtr("Tracks"),
            name: "tracks" ,
            url: "qrc:///medialibrary/MusicTracksDisplay.qml"
        }, {
            displayText: i18n.qtr("Genres"),
            name: "genres" ,
            url: "qrc:///medialibrary/MusicGenresDisplay.qml"
        }, {
            displayText: i18n.qtr("Playlists"),
            name: "playlists" ,
            url: "qrc:///medialibrary/MusicPlaylistsDisplay.qml"
        }
    ]

    onCurrentItemChanged: {
        sortModel = currentItem.sortModel
        contentModel = currentItem.model
        isViewMultiView = currentItem.isViewMultiView === undefined || currentItem.isViewMultiView
    }

    function loadIndex(index) {
        history.push(["mc", "music", root.pageModel[index].name])
    }

    property var tabModel: ListModel {
        Component.onCompleted: {
            pageModel.forEach(function(e) {
                append({
                           displayText: e.displayText,
                           name: e.name,
                       })
            })
        }
    }

    property Component localMenuDelegate: Widgets.LocalTabBar {
        currentView: root.view
        model: tabModel

        onClicked: root.loadIndex(index)
    }
}
