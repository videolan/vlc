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

Widgets.NavigableFocusScope {
    id: root

    property var extraLocalActions: undefined
    property bool isViewMultiView: true
    property var tree: undefined
    onTreeChanged:  loadView()
    Component.onCompleted: loadView()

    property var contentModel
    property var sortModel

    //reset view
    function loadDefaultView() {
        root.tree = undefined
    }

    property Component localMenuDelegate

    function loadView() {
        var page = "";
        var props = undefined;
        if (root.tree === undefined) {
            page ="qrc:///network/NetworkHomeDisplay.qml"
            root.localMenuDelegate = null
            isViewMultiView = false
        } else {
            page = "qrc:///network/NetworkBrowseDisplay.qml"
            props = { providerModel: mediaModel, contextMenu: mediaContextMenu, tree: root.tree }
            root.localMenuDelegate = addressBar
            isViewMultiView = true
        }
        view.replace(page, props)
        if (view.currentItem.model)
            root.contentModel = view.currentItem.model
        root.sortModel = view.currentItem.sortModel
    }

    Component {
        id: addressBar

        NetworkAddressbar {
            path: mediaModel.path

            onHomeButtonClicked: history.push(["mc", "network"])
        }
    }

    NetworkMediaModel {
        id: mediaModel

        ctx: mainctx
    }

    NetworkMediaContextMenu {
        id: mediaContextMenu

        model: mediaModel
    }

    Widgets.StackViewExt {
        id: view
        anchors.fill:parent
        clip: true
        focus: true

        onCurrentItemChanged: {
            extraLocalActions = view.currentItem.extraLocalActions
            view.currentItem.navigationParent = root
        }
    }
}
