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

    function loadIndex(index) {
        stackView.replace(root.pageModel[index].component)
        history.push(["mc", "music", root.pageModel[index].name], History.Stay)
        stackView.focus = true
        sortModel = stackView.currentItem.sortModel
        contentModel = stackView.currentItem.model
    }

    Component { id: albumComp; MusicAlbumsDisplay{ } }
    Component { id: artistComp; MusicArtistsDisplay{ } }
    Component { id: genresComp; MusicGenresDisplay{ } }
    Component { id: tracksComp; MusicTrackListDisplay{ } }

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

            Component.onCompleted: {
                var found = stackView.loadView(root.pageModel, view, viewProperties)
                sortModel = stackView.currentItem.sortModel
                contentModel = stackView.currentItem.model
                if (!found)
                    replace(pageModel[0].component)
            }
        }

        Connections {
            target: stackView.currentItem
            ignoreUnknownSignals: true
            onActionLeft:   root.navigationLeft(index)
            onActionRight:  root.navigationRight(index)
            onActionDown:   root.navigationDown(index)
            onActionUp:     root.navigationUp(index)
            onActionCancel: root.navigationCancel(index)
        }
    }
}
