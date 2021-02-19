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

import QtQuick          2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts  1.3
import QtQml.Models     2.2

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    //---------------------------------------------------------------------------------------------
    // Aliases
    //---------------------------------------------------------------------------------------------

    property bool isViewMultiView: true

    property variant model
    property variant sortModel

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    defaultPage: "all"

    pageModel: [{
        name: "all",
        component: componentAll
    }, {
        name: "list",
        component: componentList
    }]

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onCurrentItemChanged: {
        model     = currentItem.model;
        sortModel = currentItem.sortModel;

        isViewMultiView = (currentItem.isViewMultiView === undefined
                           ||
                           currentItem.isViewMultiView);
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------
    // Private

    function _updateHistoryList(index) {
        history.update(["mc", "video", "playlists", "all", { "initialIndex": index }]);
    }

    function _updateHistoryPlaylist(playlist) {
        history.update(["mc", "video", "playlists", "list", {
                            "initialIndex": playlist.currentIndex,
                            "initialId"   : playlist.parentId,
                            "initialName" : playlist.name
                        }]);
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Component {
        id: componentAll

        PlaylistMediaList {
            anchors.fill: parent

            onCurrentIndexChanged: _updateHistoryList(currentIndex)

            onShowList: history.push(["mc", "video", "playlists", "list",
                                      { parentId: model.id, name: model.name }])
        }
    }

    Component {
        id: componentList

        PlaylistMediaDisplay {
            id: playlist

            anchors.fill: parent

            onCurrentIndexChanged: _updateHistoryPlaylist(playlist)
            onParentIdChanged    : _updateHistoryPlaylist(playlist)
            onNameChanged        : _updateHistoryPlaylist(playlist)
        }
    }
}
