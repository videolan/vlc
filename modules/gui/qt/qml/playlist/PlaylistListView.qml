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

import org.videolan.vlc 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root

    property var plmodel: PlaylistListModel {
        playlistId: mainctx.playlist
    }

    //label for DnD
    Utils.DNDLabel {
        id: dragItem
        text: qsTr("%1 tracks selected").arg(delegateModel.selectedGroup.count)
    }


    /* popup side menu allowing to perform group action  */
    PlaylistMenu {
        id: overlay

        anchors.verticalCenter: root.verticalCenter
        anchors.right: view.right
        z: 2

        onMenuExit:{
            delegateModel.mode = "normal"
            view.focus = true
        }
        onClear: delegateModel.onDelete()
        onPlay: delegateModel.onPlay()
        onSelectionMode:  {
            delegateModel.mode = selectionMode ? "select" : "normal"
            view.focus = true
        }
        onMoveMode: {
            delegateModel.mode = moveMode ? "move" : "normal"
            view.focus = true
        }
    }

    //model

    Utils.SelectableDelegateModel {
        id: delegateModel
        model: root.plmodel

        property string mode: "normal"

        delegate: Package {
            id: element

            PLItem {
                id: plitem
                Package.name: "list"
                width: root.width
                color: VLCStyle.colors.getBgColor(element.DelegateModel.inSelected, plitem.hovered, plitem.activeFocus)

                dragitem: dragItem

                onItemClicked : {
                    view.forceActiveFocus()
                    if (delegateModel.mode == "move") {
                        delegateModel.onMoveSelectionAtPos(index)
                        view.currentIndex = index
                    } else if ( delegateModel.mode == "select" ) {
                    } else {
                        delegateModel.onUpdateIndex( modifier , view.currentIndex, index)
                        view.currentIndex = index
                    }
                }
                onItemDoubleClicked:  delegateModel.onAction(index, true)

                onDropedMovedAt: {
                    if (drop.hasUrls) {
                        delegateModel.onDropUrlAtPos(drop.urls, target)
                    } else {
                        delegateModel.onMoveSelectionAtPos(target)
                    }
                }
            }
        }

        function onMoveSelectionAtPos(target) {
            var list = []
            for (var i = 0; i < delegateModel.selectedGroup.count; i++ ) {
                list.push(delegateModel.selectedGroup.get(i).itemsIndex)
            }
            root.plmodel.moveItems(list, target)
        }

        function onDropMovedAtEnd() {
            onMoveSelectionAtPos(items.count)
        }

        function onDropUrlAtPos(urls, target) {
            var list = []
            for (var i = 0; i < urls.length; i++){
                list.push(urls[i])
            }
            mainPlaylistController.insert(target, list)
        }

        function onDropUrlAtEnd(urls) {
            var list = []
            for (var i = 0; i < urls.length; i++){
                list.push(urls[i])
            }
            mainPlaylistController.append(list)
        }

        function onDelete() {
            var list = []
            for (var i = 0; i < delegateModel.selectedGroup.count; i++ ) {
                list.push(delegateModel.selectedGroup.get(i).itemsIndex)
            }
            root.plmodel.removeItems(list)
        }

        function onPlay() {
            if (delegateModel.selectedGroup.count > 0)
                mainPlaylistController.goTo(delegateModel.selectedGroup.get(0).itemsIndex, true)
        }

        function onAction(index) {
            if (mode === "select")
                updateSelection( Qt.ControlModifier, index, view.currentIndex )
            else //normal
                onPlay()
        }

        function onUpdateIndex( keyModifiers, oldIndex, newIndex )
        {
            if (delegateModel.mode === "select") {
                console.log("update selection select")
            } else if (delegateModel.mode === "move") {
                if (delegateModel.selectedGroup.count === 0)
                    return

                var list = []
                for (var i = 0; i < delegateModel.selectedGroup.count; i++ ) {
                    list.push(delegateModel.selectedGroup.get(i).itemsIndex)
                }
                var minIndex= delegateModel.selectedGroup.get(0).itemsIndex
                var maxIndex= delegateModel.selectedGroup.get(delegateModel.selectedGroup.count - 1).itemsIndex

                if (newIndex > oldIndex) {
                    //after the next item
                    newIndex = Math.min(maxIndex + 2, delegateModel.items.count)
                    view.currentIndex = Math.min(maxIndex, delegateModel.items.count)
                } else if (newIndex < oldIndex) {
                    //before the previous item
                    view.currentIndex = Math.max(minIndex, 0)
                    newIndex = Math.max(minIndex - 1, 0)
                }

                root.plmodel.moveItems(list, newIndex)

            } else  { //normal
                updateSelection( keyModifiers, oldIndex, newIndex )
            }
        }
    }

    Utils.KeyNavigableListView {
        id: view

        anchors.fill: parent
        focus: true

        model: delegateModel.parts.list
        modelCount: delegateModel.items.count

        footer: PLItemFooter {}

        onSelectAll: delegateModel.selectAll()
        onSelectionUpdated: delegateModel.onUpdateIndex( keyModifiers, oldIndex, newIndex )
        Keys.onDeletePressed: delegateModel.onDelete()
        onActionAtIndex: delegateModel.onAction(index)
        onActionRight: {
            overlay.state = "normal"
            overlay.focus = true
        }
        onActionLeft: this.onCancel(index, root.actionLeft)
        onActionCancel: this.onCancel(index, root.actionCancel)
        onActionUp: root.actionUp(index)
        onActionDown: root.actionDown(index)

        function onCancel(index, fct) {
            if (delegateModel.mode === "select" || delegateModel.mode === "move")
            {
                overlay.state = "hidden"
                delegateModel.mode = "normal"
            }
            else
            {
                fct(index)
            }
        }

        Connections {
            target: root.plmodel
            onCurrentIndexChanged: {
                var plIndex = root.plmodel.currentIndex
                if (view.currentIndex === -1 && plIndex >= 0) {
                    delegateModel.items.get(plIndex).inSelected = true
                    view.currentIndex = plIndex
                }
            }
        }
        Connections {
            target: delegateModel.items
            onCountChanged: {
                if (view.currentIndex === -1 && delegateModel.items.count > 0) {
                    delegateModel.items.get(0).inSelected = true
                    view.currentIndex = 0
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: delegateModel.items.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("playlist is empty")
    }

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: view
    Keys.onPressed: defaultKeyAction(event, 0)
}
