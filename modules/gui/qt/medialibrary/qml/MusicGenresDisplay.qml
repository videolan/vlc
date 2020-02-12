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

Widgets.NavigableFocusScope {
    id: root

    //name and properties of the tab to be initially loaded
    property string view: "all"
    property var viewProperties: ({})

    property var sortModel
    property var contentModel

    readonly property var pageModel: [{
        name: "all",
        component: genresComponent
    }, {
        name: "albums",
        component: albumGenreComponent
    }]

    Component.onCompleted: loadView()
    onViewChanged: {
        viewProperties = {}
        loadView()
    }
    onViewPropertiesChanged: loadView()

    function loadDefaultView() {
        root.view = "all"
        root.viewProperties= ({})
    }

    function loadView() {
        var found = stackView.loadView(root.pageModel, view, viewProperties)
        if (!found)
            stackView.replace(root.pageModel[0].component)
        stackView.currentItem.navigationParent = root
        sortModel = stackView.currentItem.sortModel
        contentModel = stackView.currentItem.model
    }

    function _updateGenresAllHistory(currentIndex) {
        history.update(["mc", "music", "genres", "all", { "initialIndex": currentIndex }])
    }

    function _updateGenresAlbumsHistory(currentIndex, parentId, genreName) {
        history.update(["mc","music", "genres", "albums", {
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
        }
    }

    Component {
        id: albumGenreComponent
        /* List View */
        MusicAlbums {
            property string genreName: ""

            header: Widgets.LabelSeparator {
                text: i18n.qtr("Genres - %1").arg(genreName)
                width: root.width
            }

            onParentIdChanged: _updateGenresAlbumsHistory(currentIndex, parentId, genreName)
            onGenreNameChanged: _updateGenresAlbumsHistory(currentIndex, parentId, genreName)
            onCurrentIndexChanged: _updateGenresAlbumsHistory(currentIndex, parentId, genreName)
        }
    }

    Widgets.StackViewExt {
        id: stackView

        anchors.fill: parent
        focus: true
    }
}
