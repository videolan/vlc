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
import QtQml.Models 2.2
import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    property var sortModel
    property var model

    pageModel: [{
        name: "all",
        component: genresComponent
    }, {
        name: "albums",
        component: albumGenreComponent
    }]

    loadDefaultView: function () {
        History.update(["mc", "music", "genres", "all"])
        loadPage("all")
    }

    onCurrentItemChanged: {
        sortModel = currentItem.sortModel
        model = currentItem.model
    }


    function _updateGenresAllHistory(currentIndex) {
        History.update(["mc", "music", "genres", "all", { "initialIndex": currentIndex }])
    }

    function _updateGenresAlbumsHistory(currentIndex, parentId, genreName) {
        History.update(["mc","music", "genres", "albums", {
            "initialIndex": currentIndex,
            "parentId": parentId,
            "genreName": genreName,
        }])
    }

    Component {
        id: genresComponent
        /* List View */
        MusicGenres {
            onCurrentIndexChanged: _updateGenresAllHistory(currentIndex)

            onShowAlbumView: {
                History.push(["mc", "music", "genres", "albums",
                             { parentId: id, genreName: name }]);

                stackView.currentItem.setCurrentItemFocus(reason);
            }
        }
    }

    Component {
        id: albumGenreComponent
        /* List View */
        MusicAlbums {
            id: albumsView

            property string genreName: ""

            gridViewMarginTop: 0

            header: Widgets.SubtitleLabel {
                text: I18n.qtr("Genres - %1").arg(genreName)
                leftPadding: albumsView.contentLeftMargin
                rightPadding: albumsView.contentRightMargin
                topPadding: VLCStyle.margin_large
                bottomPadding: VLCStyle.margin_normal
                width: root.width
            }

            onParentIdChanged: _updateGenresAlbumsHistory(currentIndex, parentId, genreName)
            onGenreNameChanged: _updateGenresAlbumsHistory(currentIndex, parentId, genreName)
            onCurrentIndexChanged: _updateGenresAlbumsHistory(currentIndex, parentId, genreName)
        }
    }
}
