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
import QtQuick.Controls 2.4
import QtQuick 2.11
import QtQml.Models 2.2
import QtQuick.Layouts 1.11

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"


Widgets.PageLoader {
    id: root

    property MLModel model

    pageModel: [{
        name: "all",
        component: allArtistsComponent
    }, {
        name: "albums",
        component: artistAlbumsComponent
    }]

    loadDefaultView: function () {
        History.update(["mc", "music", "artists", "all"])
        loadPage("all")
    }

    onCurrentItemChanged: {
        model = currentItem.model
    }

    function _updateArtistsAllHistory(currentIndex) {
        History.update(["mc", "music", "artists", "all", { "initialIndex": currentIndex }])
    }

    function _updateArtistsAlbumsHistory(currentIndex, initialAlbumIndex) {
        History.update(["mc","music", "artists", "albums", {
            "initialIndex": currentIndex,
            "initialAlbumIndex": initialAlbumIndex,
        }])
    }

    Component {
        id: allArtistsComponent

        MusicAllArtists {
            onCurrentIndexChanged: _updateArtistsAllHistory(currentIndex)

            onRequestArtistAlbumView: {
                History.push(["mc", "music", "artists", "albums",
                              { initialIndex: currentIndex } ]);

                stackView.currentItem.setCurrentItemFocus(reason);
            }
        }
    }

    Component {
        id: artistAlbumsComponent

        MusicArtistsAlbums {

            Navigation.parentItem: root

            onCurrentIndexChanged: _updateArtistsAlbumsHistory(currentIndex, currentAlbumIndex)
            onCurrentAlbumIndexChanged: _updateArtistsAlbumsHistory(currentIndex, currentAlbumIndex)
        }
    }
}
