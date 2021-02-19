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
    // Properties
    //---------------------------------------------------------------------------------------------

    property bool isViewMultiView: true

    property var contentModel
    property var sortModel

    property var tabModel: ListModel {
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

        onClicked: root.loadIndex(index)
    }

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    defaultPage: "all"

    pageModel: [{
            name: "all",
            displayText: i18n.qtr("All"),
            url: "qrc:///medialibrary/VideoAllDisplay.qml"
        }, {
            name: "playlists",
            displayText: i18n.qtr("Playlists"),
            url: "qrc:///medialibrary/VideoPlaylistsDisplay.qml"
        }
    ]

    onCurrentItemChanged: {
        isViewMultiView = (currentItem.isViewMultiView === undefined
                           ||
                           currentItem.isViewMultiView);

        contentModel = currentItem.model;
        sortModel    = currentItem.sortModel;
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function loadIndex(index) {
        history.push(["mc", "video", root.pageModel[index].name]);
    }
}
