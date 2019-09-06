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
    }


    /* popup side menu allowing to perform group action  */
    PlaylistMenu {
        id: overlay

        anchors.verticalCenter: root.verticalCenter
        anchors.right: view.right
        z: 2

        onMenuExit:{
            view.mode = "normal"
            view.focus = true
        }
        onClear: view.onDelete()
        onPlay: view.onPlay()
        onSelectionMode:  {
            view.mode = selectionMode ? "select" : "normal"
            view.focus = true
        }
        onMoveMode: {
            view.mode = moveMode ? "move" : "normal"
            view.focus = true
        }
    }

    Utils.KeyNavigableListView {
        id: view

        anchors.fill: parent
        focus: true

        model: root.plmodel
        modelCount: root.plmodel.count

        property int shiftIndex: -1
        property string mode: "normal"

        Connections {
            target: root.plmodel
            onRowsInserted: {
                if (view.currentIndex == -1)
                    view.currentIndex = 0
            }
            onModelReset: {
                if (view.currentIndex == -1 &&  root.plmodel.count > 0)
                    view.currentIndex = 0
            }
        }

        footer: PLItemFooter {}

        delegate: PLItem {
            /*
             * implicit variables:
             *  - model: gives access to the values associated to PlaylistListModel roles
             *  - index: the index of this item in the list
             */
            id: plitem
            plmodel: root.plmodel
            width: root.width

            onItemClicked : {
                /* to receive keys events */
                view.forceActiveFocus()
                if (view.mode == "move") {
                    var selectedIndexes = root.plmodel.getSelection()
                    var preTarget = index
                    /* move to _above_ the clicked item if move up, but
                     * _below_ the clicked item if move down */
                    if (preTarget > selectedIndexes[0])
                        preTarget++
                    view.currentIndex = selectedIndexes[0]
                    root.plmodel.moveItemsPre(selectedIndexes, preTarget)
                } else if (view.mode == "select") {
                } else {
                    view.updateSelection(modifier, view.currentIndex, index)
                    view.currentIndex = index
                }
            }
            onItemDoubleClicked: mainPlaylistController.goTo(index, true)
            color: VLCStyle.colors.getBgColor(model.selected, plitem.hovered, plitem.activeFocus)

            onDragStarting: {
                if (!root.plmodel.isSelected(index)) {
                    /* the dragged item is not in the selection, replace the selection */
                    root.plmodel.setSelection([index])
                }
            }

            onDropedMovedAt: {
                if (drop.hasUrls) {
                    mainPlaylistController.insert(target, drop.urls)
                } else {
                    root.plmodel.moveItemsPre(root.plmodel.getSelection(), target)
                }
            }
        }

        onSelectAll: root.plmodel.selectAll()
        onSelectionUpdated: {
            if (view.mode === "select") {
                console.log("update selection select")
            } else if (mode == "move") {
                var selectedIndexes = root.plmodel.getSelection()

                /* always move relative to the first item of the selection */
                var target = selectedIndexes[0];
                if (newIndex > oldIndex) {
                    /* move down */
                    target++
                } else if (newIndex < oldIndex && target > 0) {
                    /* move up */
                    target--
                }

                view.currentIndex = selectedIndexes[0]
                /* the target is the position _after_ the move is applied */
                root.plmodel.moveItemsPost(selectedIndexes, target)
            } else { // normal
                updateSelection(keyModifiers, oldIndex, newIndex);
            }
        }

        Keys.onDeletePressed: onDelete()

        navigationParent: root
        navigationRight: function() {
            overlay.state = "normal"
            overlay.focus = true
        }
        navigationLeft: function(index) {
            if (mode === "normal") {
                root.navigationLeft(index)
            } else {
                overlay.state = "hidden"
                mode = "normal"
            }
        }
        navigationCancel: function(index) {
            if (mode === "normal") {
                root.navigationCancel(index)
            } else {
                overlay.state = "hidden"
                mode = "normal"
            }
        }

        onActionAtIndex: {
            if (mode === "select")
                root.plmodel.toggleSelected(index)
            else //normal
                // play
                mainPlaylistController.goTo(index, true)
        }

        function onPlay() {
            let selection = root.plmodel.getSelection()
            if (selection.length > 0)
                mainPlaylistController.goTo(selection[0], true)
        }

        function onDelete() {
            root.plmodel.removeItems(root.plmodel.getSelection())
        }

        function _addRange(from, to) {
            root.plmodel.setRangeSelected(from, to - from + 1, true)
        }

        function _delRange(from, to) {
            root.plmodel.setRangeSelected(from, to - from + 1, false)
        }

        // copied from SelectableDelegateModel, which is intended to be removed
        function updateSelection( keymodifiers, oldIndex, newIndex ) {
            if ((keymodifiers & Qt.ShiftModifier)) {
                if ( shiftIndex === oldIndex) {
                    if ( newIndex > shiftIndex )
                        _addRange(shiftIndex, newIndex)
                    else
                        _addRange(newIndex, shiftIndex)
                } else if (shiftIndex <= newIndex && newIndex < oldIndex) {
                    _delRange(newIndex + 1, oldIndex )
                } else if ( shiftIndex < oldIndex && oldIndex < newIndex ) {
                    _addRange(oldIndex, newIndex)
                } else if ( newIndex < shiftIndex && shiftIndex < oldIndex ) {
                    _delRange(shiftIndex, oldIndex)
                    _addRange(newIndex, shiftIndex)
                } else if ( newIndex < oldIndex && oldIndex < shiftIndex  ) {
                    _addRange(newIndex, oldIndex)
                } else if ( oldIndex <= shiftIndex && shiftIndex < newIndex ) {
                    _delRange(oldIndex, shiftIndex)
                    _addRange(shiftIndex, newIndex)
                } else if ( oldIndex < newIndex && newIndex <= shiftIndex  ) {
                    _delRange(oldIndex, newIndex - 1)
                }
            } else {
                shiftIndex = newIndex
                if (keymodifiers & Qt.ControlModifier) {
                    root.plmodel.toggleSelected(newIndex)
                } else {
                    root.plmodel.setSelection([newIndex])
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: plmodel.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("playlist is empty")
    }

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: view
    Keys.onPressed: defaultKeyAction(event, 0)
}
