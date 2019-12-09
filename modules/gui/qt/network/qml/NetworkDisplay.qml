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
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property var extraLocalActions: undefined

    property var tree: undefined
    onTreeChanged:  loadView()
    Component.onCompleted: loadView()

    //reset view
    function loadDefaultView() {
        root.tree = undefined
    }

    function loadView() {
        var page = "";
        if (root.tree === undefined)
            page ="qrc:///network/NetworkHomeDisplay.qml"
        else
            page = "qrc:///network/NetworkBrowseDisplay.qml"
        view.replace(page)
        if (root.tree) {
            view.currentItem.tree = root.tree
        }
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
