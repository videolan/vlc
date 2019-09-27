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

import "qrc:///utils/" as Utils
import "qrc:///style/"

import org.videolan.medialib 0.1

Utils.NavigableFocusScope {
    id: root

    //name and properties of the tab to be initially loaded
    property string view: "albums"
    property var viewProperties: ({})

    property var sortModel
    property var contentModel

    onViewChanged: loadView()
    onViewProperties: loadView()
    Component.onCompleted: loadView()

    function loadView() {
        var found = stackView.loadView(root.pageModel, view, viewProperties)
        if (!found)
            stackView.replace(root.pageModel[0].component)
        sortModel = stackView.currentItem.sortModel
        contentModel = stackView.currentItem.model
    }

    //reset view
    function loadDefaultView() {
        root.view = "albums"
        root.viewProperties= ({})
    }

    function loadIndex(index) {
        history.push(["mc", "music", root.pageModel[index].name], History.Go)
    }

    Component { id: albumComp; MusicAlbumsDisplay{ navigationParent: root } }
    Component { id: artistComp; MusicArtistsDisplay{ navigationParent: root } }
    Component { id: genresComp; MusicGenresDisplay{ navigationParent: root } }
    Component { id: tracksComp; MusicTrackListDisplay{  navigationParent: root } }

    readonly property var pageModel: [{
            displayText: qsTr("Albums"),
            name: "albums",
            component: albumComp
        }, {
            displayText: qsTr("Artists"),
            name: "artists",
            component: artistComp
        }, {
            displayText: qsTr("Genres"),
            name: "genres" ,
            component: genresComp
        }, {
            displayText: qsTr("Tracks"),
            name: "tracks" ,
            component: tracksComp
        }
    ]

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

    ColumnLayout {
        anchors.fill : parent
        spacing: 0

        /* The data elements */
        Utils.StackViewExt  {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: VLCStyle.margin_normal
            focus: true

            onCurrentItemChanged: {
                sortModel = stackView.currentItem.sortModel
                contentModel = stackView.currentItem.model
            }
        }
    }
}
