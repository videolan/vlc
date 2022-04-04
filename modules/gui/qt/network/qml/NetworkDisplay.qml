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
import QtQml 2.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    // Properties

    property var sortModel
    property var contentModel
    property bool isViewMultiView: true

    property Component localMenuDelegate

    // Settings

    pageModel: [{
        name: "home",
        url: "qrc:///network/NetworkHomeDisplay.qml"
    }, {
        name: "browse",
        component: browseComponent,
        guard: function (prop) { return !!prop.tree }
    }]

    loadDefaultView: function() {
        History.update(["mc", "network", "home"])
        loadPage("home")
    }

    // Events
    onCurrentItemChanged: {
        sortModel = currentItem.sortModel;
        contentModel = currentItem.model;

        isViewMultiView = (currentItem.isViewMultiView === undefined
                           ||
                           currentItem.isViewMultiView);

        if (view.name === "browse")
            localMenuDelegate = componentBar
        else
            localMenuDelegate = null
    }

    // Connections

    Connections {
        target: stackView.currentItem

        onBrowse: {
            History.push(["mc", "network", "browse", { tree: tree }]);
            stackView.currentItem.setCurrentItemFocus(reason);
        }
    }

    // Children

    Component {
        id: browseComponent

        NetworkBrowseDisplay {
            providerModel: NetworkMediaModel {
                ctx: MainCtx
            }

            contextMenu: NetworkMediaContextMenu {
                model: providerModel
            }
        }
    }

    Component {
        id: componentBar

        NetworkAddressbar {
            path: view.name === "browse" ? root.stackView.currentItem.providerModel.path : []

            onHomeButtonClicked: {
                History.push(["mc", "network", "home"])

                stackView.currentItem.setCurrentItemFocus(reason)
            }

            onBrowse: {
                History.push(["mc", "network", "browse", { "tree": tree }])

                stackView.currentItem.setCurrentItemFocus(reason)
            }
        }
    }
}
