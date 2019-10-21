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

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.SelectableDelegateModel {
    id: delegateModel
    property int currentIndex: -1

    property string viewIndexPropertyName: "currentIndex"
    delegate: Package {
        id: element
        NetworkGridItem {
            id: delegateGrid
            focus: true
            Package.name: "grid"

            onItemClicked : {
                delegateModel.updateSelection( modifier ,  delegateModel.currentIndex, index)
                delegateModel.currentIndex = index
                delegateGrid.forceActiveFocus()
            }

            onItemDoubleClicked: {
                if (model.type === MLNetworkMediaModel.TYPE_NODE || model.type === MLNetworkMediaModel.TYPE_DIRECTORY)
                    history.push( ["mc", "network", { tree: model.tree } ], History.Go)
                else
                    delegateModel.model.addAndPlay( index )
            }

            onContextMenuButtonClicked: {
                contextMenu.model = model
                contextMenu.delegateModel = delegateModel
                contextMenu.popup(menuParent)
            }
        }

        NetworkListItem {
            id: delegateList
            focus: true
            Package.name: "list"

            onItemClicked : {
                delegateModel.updateSelection( modifier, delegateModel.currentIndex, index )
                delegateModel.currentIndex = index
                delegateList.forceActiveFocus()
            }

            onItemDoubleClicked: {
                if (model.type === MLNetworkMediaModel.TYPE_NODE || model.type === MLNetworkMediaModel.TYPE_DIRECTORY)
                    history.push( ["mc", "network", { tree: model.tree } ], History.Go)
                else
                    delegateModel.model.addAndPlay( index )
            }

            onContextMenuButtonClicked: {
                contextMenu.model = model
                contextMenu.delegateModel = delegateModel
                contextMenu.popup(menuParent)
            }

            onActionLeft: root.navigationLeft(0)
            onActionRight: root.navigationRight(0)
        }

    }

    function switchIndex() {
        var list = []
        for (var i = 0; i < delegateModel.selectedGroup.count; i++) {
            var obj = delegateModel.selectedGroup.get(i)
            if (obj.model.can_index) {
                obj.model.indexed = !obj.model.indexed
            }
        }
    }

    function playSelection() {
        var list = []
        for (var i = 0; i < delegateModel.selectedGroup.count; i++) {
            var index = delegateModel.selectedGroup.get(i).itemsIndex;
            list.push(index)
        }
        delegateModel.model.addAndPlay( list )
    }

    function enqueueSelection() {
        var list = []
        for (var i = 0; i < delegateModel.selectedGroup.count; i++) {
            var index = delegateModel.selectedGroup.get(i).itemsIndex;
            list.push(index)
        }
        delegateModel.model.addToPlaylist( list )
    }

    function actionAtIndex(index) {
        if ( delegateModel.selectedGroup.count > 1 ) {
            playSelection()
        } else {
            if (delegateModel.items.get(index).model.type === MLNetworkMediaModel.TYPE_DIRECTORY
                    || delegateModel.items.get(index).model.type === MLNetworkMediaModel.TYPE_NODE)  {
                console.log("push network tree", delegateModel.items.get(index).model.tree)
                history.push(["mc", "network", { tree: delegateModel.items.get(index).model.tree }], History.Go);
            } else {
                playSelection()
            }
        }
    }
}
