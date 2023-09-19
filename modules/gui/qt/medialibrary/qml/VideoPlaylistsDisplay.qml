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
import QtQml.Models

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
    // Childs
    //---------------------------------------------------------------------------------------------

    Component {
        id: componentAll

        PlaylistMediaList {
            id: playlistView

            header: Widgets.ViewHeader {
                view: playlistView

                text: qsTr("Playlists")
            }

            isMusic: false

            searchPattern: MainCtx.search.pattern
            sortOrder: MainCtx.sort.order
            sortCriteria: MainCtx.sort.criteria

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            onShowList: (model, reason) => {
                History.push([...root.pagePrefix, "list"], { parentId: model.id, name: model.name }, reason)
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

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
        }
    }
}
