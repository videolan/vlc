/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
    // Aliases
    //---------------------------------------------------------------------------------------------

    property bool isViewMultiView: true

    property variant model
    property variant sortModel

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    defaultPage: "all"

    pageModel: [{
        name: "all",
        component: componentAll
    }, {
        name: "list",
        component: componentList
    }]

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onCurrentItemChanged: {
        model     = currentItem.model;
        sortModel = currentItem.sortModel;

        isViewMultiView = (currentItem.isViewMultiView === undefined
                           ||
                           currentItem.isViewMultiView);
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------
    // Private

    function _updateHistoryAll(index) {
        history.update(["mc", "video", "groups", "all", { "initialIndex": index }]);
    }

    function _updateHistoryList(list) {
        history.update(["mc", "video", "groups", "list", {
                            "initialIndex": list.currentIndex,
                            "initialId"   : list.parentId,
                            "initialName" : list.name
                        }]);
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Component {
        id: componentAll

        MediaGroupList {
            anchors.fill: parent

            onCurrentIndexChanged: _updateHistoryAll(currentIndex)

            onShowList: history.push(["mc", "video", "groups", "list",
                                      { parentId: model.id, name: model.name }])
        }
    }

    Component {
        id: componentList

        MediaGroupDisplay {
            id: list

            anchors.fill: parent

            onCurrentIndexChanged: _updateHistoryList(list)
            onParentIdChanged    : _updateHistoryList(list)
            onNameChanged        : _updateHistoryList(list)
        }
    }
}
