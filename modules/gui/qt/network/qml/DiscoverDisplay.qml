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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQml.Models 2.2
import QtQml 2.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    property var sortModel
    property var contentModel

    //name and properties of the tab to be initially loaded
    defaultPage: "services"

    pageModel: [{
            displayText: i18n.qtr("Services"),
            name: "services",
            url: "qrc:///network/ServicesHomeDisplay.qml"
        }, {
            displayText: i18n.qtr("URL"),
            name: "url",
            url: "qrc:/medialibrary/UrlListDisplay.qml"
        }
    ]

    onCurrentItemChanged: {
        sortModel = currentItem.sortModel
        contentModel = currentItem.model
    }


    function loadIndex(index) {
        history.push(["mc", "discover", root.pageModel[index].name])
    }


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
}
