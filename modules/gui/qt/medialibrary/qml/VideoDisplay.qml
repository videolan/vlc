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

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Style

Widgets.PageLoader {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------
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

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    localMenuDelegate: Widgets.LocalTabBar {
        currentView: root.pageName

        model: tabModel

        onClicked: (index) => {
            const pageName = root.pageModel[index].name
            if (root.isDefaulLoadedForPath([pageName]))
                return
            History.push([...root.pagePrefix, pageName])
        }
    }

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    pageModel: [{
            name: "all",
            default: true,
            displayText: qsTr("All"),
            url: "qrc:///qt/qml/VLC/MediaLibrary/VideoAllDisplay.qml"
        },{
            name: "playlists",
            displayText: qsTr("Playlists"),
            url: "qrc:///qt/qml/VLC/MediaLibrary/VideoPlaylistsDisplay.qml"
        }
    ]

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Video view")

    onCurrentItemChanged: {
        // NOTE: We need bindings because the VideoAll model can change over time.
        sortMenu     = Qt.binding(function () { return currentItem.sortMenu; })

        if (currentItem) {
            if (currentItem.displayMarginBeginning !== undefined)
                currentItem.displayMarginBeginning = Qt.binding(() => { return root.displayMarginBeginning })

            if (currentItem.displayMarginEnd !== undefined)
                currentItem.displayMarginEnd = Qt.binding(() => { return root.displayMarginEnd })

            if (currentItem.enableBeginningFade !== undefined)
                currentItem.enableBeginningFade = Qt.binding(() => { return root.enableBeginningFade })

            if (currentItem.enableEndFade !== undefined)
                currentItem.enableEndFade = Qt.binding(() => { return root.enableEndFade })
        }
    }
}
