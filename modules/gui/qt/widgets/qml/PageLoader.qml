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
import QtQuick 2.0

NavigableFocusScope {
    id: root

    property string view: defaultPage
    property var viewProperties: ({})

    property string defaultPage: ""

    property var pageModel: []

    signal pageChanged(string page)
    signal currentItemChanged(var currentItem)

    Component.onCompleted: loadView()
    onViewChanged: {
        viewProperties = {}
        loadView()
    }
    onViewPropertiesChanged: loadView()

    function loadDefaultView() {
        root.view = defaultPage
        root.viewProperties = ({})
    }

    function loadView() {
        if (view === "") {
            console.error("view is not defined")
            return
        }
        if (pageModel === []) {
            console.error("pageModel is not defined")
            return
        }
        var found = stackView.loadView(root.pageModel, view, viewProperties)
        if (!found)
            stackView.replace(root.pageModel[0].component)

        if (stackView.currentItem && stackView.currentItem.hasOwnProperty("navigationParent")) {
            stackView.currentItem.navigationParent = root
        }
        root.currentItemChanged(stackView.currentItem)
    }

    StackViewExt {
        id: stackView

        anchors.fill: parent
        focus: true
    }
}
