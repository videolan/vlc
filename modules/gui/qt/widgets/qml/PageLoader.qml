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
import org.videolan.vlc 0.1

FocusScope {
    id: root

    property var view: null
    property string defaultPage: ""

    property var pageModel: []

    property alias stackView: stackView

    signal pageChanged(string page)
    signal currentItemChanged(var currentItem)

    Component.onCompleted: loadView()
    onViewChanged: {
        loadView()
    }

    function loadView() {
        if (view === null) {
            var defaultView = {"name": defaultPage, "properties": {}}
            History.addLeaf({"view": defaultView})
            root.view = defaultView
            return
        }

        if (view.name === "") {
            console.error("view is not defined")
            return
        }
        if (pageModel === []) {
            console.error("pageModel is not defined")
            return
        }
        var found = stackView.loadView(root.pageModel, view.name, view.properties)
        if (!found) {
            console.error("failed to load", JSON.stringify(History.current))
            return
        }

        stackView.currentItem.Navigation.parentItem = root
        root.currentItemChanged(stackView.currentItem)
    }

    function setCurrentItemFocus(reason) {
        stackView.setCurrentItemFocus(reason);
    }

    StackViewExt {
        id: stackView

        anchors.fill: parent
        focus: true
    }
}
