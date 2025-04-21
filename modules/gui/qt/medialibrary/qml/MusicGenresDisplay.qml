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
import QtQml.Models
import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Util
import VLC.Widgets as Widgets
import VLC.Style

Widgets.PageLoader {
    id: root

    pageModel: [{
        name: "all",
        default: true,
        component: genresComponent
    }, {
        name: "albums",
        component: albumGenreComponent
    }]

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    Component {
        id: genresComponent
        /* List View */
        MusicGenres {
            id: genresView

            header: Widgets.ViewHeader {
                view: genresView

                visible: view.count > 0

                text: qsTr("Genres")
            }

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            searchPattern: MainCtx.search.pattern
            sortOrder: MainCtx.sort.order
            sortCriteria: MainCtx.sort.criteria

            onShowAlbumView: (id, name, reason) => {
                History.push([...root.pagePrefix, "albums"], { parentId: id, genreName: name }, reason)
            }
        }
    }

    Component {
        id: albumGenreComponent
        /* List View */
        MusicAlbums {
            id: albumsView

            property string genreName: ""

            header: Widgets.ViewHeader {
                view: albumsView

                visible: view.count > 0

                text: qsTr("Genres - %1").arg(genreName)
            }

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade

            searchPattern: MainCtx.search.pattern
            sortOrder: MainCtx.sort.order
            sortCriteria: MainCtx.sort.criteria

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
        }
    }
}
