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
import QtQuick.Controls
import QtQuick
import QtQml.Models
import QtQuick.Layouts

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"


Widgets.PageLoader {
    id: root

    pageModel: [{
        name: "all",
        default: true,
        component: allArtistsComponent
    }, {
        name: "albums",
        component: artistAlbumsComponent
    }]


    Component {
        id: allArtistsComponent

        MusicAllArtists {
            id: artistsView

            header: Widgets.ViewHeader {
                view: artistsView

                text: qsTr("Artists")
            }

            searchPattern: MainCtx.search.pattern
            sortOrder: MainCtx.sort.order
            sortCriteria: MainCtx.sort.criteria

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            onRequestArtistAlbumView: (reason) => {
                History.push([...root.pagePrefix, "albums"], { initialIndex: currentIndex }, reason)
            }
        }
    }

    Component {
        id: artistAlbumsComponent

        MusicArtistsAlbums {
            searchPattern: MainCtx.search.pattern
            sortOrder: MainCtx.sort.order
            sortCriteria: MainCtx.sort.criteria

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
            onCurrentAlbumIndexChanged: History.viewProp.initialAlbumIndex = currentAlbumIndex
        }
    }
}
