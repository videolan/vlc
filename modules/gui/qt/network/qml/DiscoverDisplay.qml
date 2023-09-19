/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQml

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    pageModel: [{
            displayText: qsTr("Services"),
            default: true,
            name: "services",
            url: "qrc:///network/ServicesHomeDisplay.qml"
        }, {
            displayText: qsTr("URL"),
            name: "url",
            url: "qrc:///network/DiscoverUrlDisplay.qml"
        }
    ]

    localMenuDelegate: menuDelegate

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Discover view")

    function loadIndex(index) {
        const pageName = root.pageModel[index].name
        if (root.isDefaulLoadedForPath([pageName]))
            return
        History.push([...root.pagePrefix, pageName])
    }

    property ListModel tabModel: ListModel {
        Component.onCompleted: {
            pageModel.forEach(function(e) {
                append({
                           displayText: e.displayText,
                           name: e.name,
                       })
            })
        }
    }

    Component {
        id: menuDelegate

        Widgets.LocalTabBar {
            currentView: root.pageName
            model: tabModel

            onClicked: (index) => root.loadIndex(index)
        }
    }
}
