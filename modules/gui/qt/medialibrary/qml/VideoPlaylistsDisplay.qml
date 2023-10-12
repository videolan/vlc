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

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2.12

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    pageModel: [{
        name: "all",
        default: true,
        component: componentAll
    }, {
        name: "list",
        component: componentList
    }]

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------
    // Private

    function _updateHistoryList(index) {
        History.update(["mc", "video", "playlists", "all"], { "initialIndex": index })
    }

    function _updateHistoryPlaylist(playlist) {
        History.update(["mc", "video", "playlists", "list"], {
                            "currentIndex": playlist.currentIndex,
                            "parentId"   : playlist.parentId,
                            "name" : playlist.name
                        });
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Component {
        id: componentAll

        PlaylistMediaList {

            isMusic: false

            searchPattern: MainCtx.search.pattern
            sortOrder: MainCtx.sort.order
            sortCriteria: MainCtx.sort.criteria

            onCurrentIndexChanged: _updateHistoryList(currentIndex)

            onShowList: (model, reason) => {
                History.push(["mc", "video", "playlists", "list"],
                             { parentId: model.id, name: model.name }, reason);
            }
        }
    }

    Component {
        id: componentList

        PlaylistMediaDisplay {
            id: playlist

            isMusic: false

            searchPattern: MainCtx.search.pattern
            sortOrder: MainCtx.sort.order
            sortCriteria: MainCtx.sort.criteria

            onCurrentIndexChanged: _updateHistoryPlaylist(playlist)
            onParentIdChanged    : _updateHistoryPlaylist(playlist)
            onNameChanged        : _updateHistoryPlaylist(playlist)
        }
    }
}
