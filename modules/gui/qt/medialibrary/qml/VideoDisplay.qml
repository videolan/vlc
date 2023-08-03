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
    // Properties
    //---------------------------------------------------------------------------------------------
    property var contentModel

    property var sortMenu

    property ListModel tabModel: ListModel {
        Component.onCompleted: {
            pageModel.forEach(function(e) {
                append({
                    name       : e.name,
                    displayText: e.displayText
                })
            })
        }
    }

    property Component localMenuDelegate: Widgets.LocalTabBar {
        currentView: root.view

        model: tabModel

        onClicked: {
            if (root.pageModel[index].name === currentView.name)
                return

            root.loadIndex(index)
        }
    }

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    pageModel: [{
            name: "all",
            displayText: I18n.qtr("All"),
            url: "qrc:///medialibrary/VideoAllDisplay.qml"
        },{
            name: "playlists",
            displayText: I18n.qtr("Playlists"),
            url: "qrc:///medialibrary/VideoPlaylistsDisplay.qml"
        }
    ]

    loadDefaultView: function () {
        History.update(["mc", "video", "all"])
        loadPage("all")
    }

    Accessible.role: Accessible.Client
    Accessible.name: I18n.qtr("Video view")

    onCurrentItemChanged: {
        // NOTE: We need bindings because the VideoAll model can change over time.
        contentModel = Qt.binding(function () { return currentItem.model; })
        sortMenu     = Qt.binding(function () { return currentItem.sortMenu; })
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function loadIndex(index) {
        History.push(["mc", "video", root.pageModel[index].name]);
    }
}
