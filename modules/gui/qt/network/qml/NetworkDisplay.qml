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

    property var tree: undefined

    property Component localMenuDelegate

    // Settings

    defaultPage: "home"

    pageModel: [{
        name: "home",
        url: "qrc:///network/NetworkHomeDisplay.qml"
    }, {
        name: "browse",
        component: browseComponent
    }]

    // Events

    onCurrentItemChanged: {
        sortModel = currentItem.sortModel;
        contentModel = currentItem.model;

        isViewMultiView = (currentItem.isViewMultiView === undefined
                           ||
                           currentItem.isViewMultiView);

        if (tree) {
            if (view == "home")
                localMenuDelegate = null;
            else
                localMenuDelegate = componentBar;
        } else {
            localMenuDelegate = null;
        }
    }

    // Functions
    // PageLoader reimplementation

    // FIXME: Maybe this could be done with a 'guard' mechanism on the pageModel.
    function loadView() {
        if (tree)
            stackView.loadView(pageModel, view, viewProperties);
        else
            stackView.loadView(pageModel, "home", viewProperties);

        stackView.currentItem.Navigation.parentItem = root;

        currentItemChanged(stackView.currentItem);
    }

    // Connections

    Connections {
        target: stackView.currentItem

        onBrowse: {
            root.tree = tree;

            history.push(["mc", "network", "browse", { tree: tree }]);

            stackView.currentItem.setCurrentItemFocus(reason);
        }
    }

    // Children

    Component {
        id: browseComponent

        NetworkBrowseDisplay {
            providerModel: NetworkMediaModel {
                ctx: mainctx
            }

            contextMenu: NetworkMediaContextMenu {
                model: providerModel
            }
        }
    }

    Component {
        id: componentBar

        NetworkAddressbar {
            path: view === "browse" ? root.stackView.currentItem.providerModel.path : []

            onHomeButtonClicked: history.push(["mc", "network", "home"])
        }
    }
}
