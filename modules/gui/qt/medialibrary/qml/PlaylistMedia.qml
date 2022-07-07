/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQml.Models     2.2

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/"    as MainInterface
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

MainInterface.MainTableView {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    readonly property int columns: VLCStyle.gridColumnsForWidth(root.availableRowWidth)

    //---------------------------------------------------------------------------------------------
    // Private

    property Item _item: null

    property bool _before: true

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    rowHeight: VLCStyle.tableCoverRow_height

    delegate: PlaylistMediaDelegate {
        id: tableDelegate

        width: view.width
        height: root.rowHeight

        horizontalSpacing: root.horizontalSpacing
        leftPadding: Math.max(0, view.width - root.usedRowSpace) / 2 + root.sectionWidth

        dragItem: root.dragItem

        selected: selectionDelegateModel.isSelected(root.model.index(index, 0))

        onContextMenuButtonClicked: root.contextMenuButtonClicked(menuParent, menuModel, globalMousePos)
        onRightClick: root.rightClick(menuParent, menuModel, globalMousePos)
        onItemDoubleClicked: root.itemDoubleClicked(index, model)

        onSelectAndFocus:  {
            selectionDelegateModel.updateSelection(modifiers, view.currentIndex, index)

            view.currentIndex = index
            view.positionViewAtIndex(index, ListView.Contain)

            tableDelegate.forceActiveFocus(focusReason)
        }

        Connections {
            target: selectionDelegateModel

            onSelectionChanged: {
                tableDelegate.selected = Qt.binding(function() {
                  return  selectionDelegateModel.isSelected(root.model.index(index, 0))
                })
            }
        }
    }

    headerColor: VLCStyle.colors.bg

    sortModel: [{
        criteria: "thumbnail",

        width: VLCStyle.colWidth(1),

        type: "image",

        headerDelegate: table.titleHeaderDelegate,
        colDelegate   : table.titleDelegate,

        placeHolder: VLCStyle.noArtAlbumCover,
    }, {
        isPrimary: true,

        criteria: "title",

        width: VLCStyle.colWidth(Math.max(columns - 2, 1)),

        text: I18n.qtr("Title")
    }, {
        criteria: "duration",

        width: VLCStyle.colWidth(1),

        headerDelegate: table.timeHeaderDelegate,
        colDelegate   : table.timeColDelegate
    }]

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onActionForSelection: MediaLib.addAndPlay(model.getIdsForIndexes(selection))
    onItemDoubleClicked: MediaLib.addAndPlay(model.id)

    //---------------------------------------------------------------------------------------------
    // Connections
    //---------------------------------------------------------------------------------------------

    Connections {
        target: root

        // NOTE: We want to hide the drop line when scrolling so its position stays relevant.
        onContentYChanged: hideLine(_item)
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------
    // Drop interface

    function isDroppable(drop, index) {
        // NOTE: Internal drop (intra-playlist).
        return Helpers.isValidInstanceOf(drop.source, Widgets.DragItem);
    }

    function applyDrop(drop, index) {
        var item = drop.source;

        // NOTE: Move implementation.
        if (dragItem === item) {
            model.move(modelSelect.selectedIndexes, index);

        // NOTE: Dropping medialibrary content into the playlist.
        } else if (Helpers.isValidInstanceOf(item, Widgets.DragItem)) {
            item.getSelectedInputItem(function(inputItems) {
                model.insert(inputItems, index);
            })
        }

        forceActiveFocus();

        root.hideLine(_item);
    }

    //---------------------------------------------------------------------------------------------
    // Drop line

    function showLine(item, before)
    {
        // NOTE: We want to avoid calling mapFromItem too many times.
        if (_item === item && _before === before)
            return;

        _item   = item;
        _before = before;

        if (before)
            line.y = view.mapFromItem(item, 0, 0).y;
        else
            line.y = view.mapFromItem(item, 0, item.height).y;

        line.visible = true;
    }

    function hideLine(item)
    {
        // NOTE: We want to make sure we're not being called after the 'showLine' function.
        if (_item !== item)
            return;

        _item = null;

        line.visible = false;
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Widgets.TableColumns {
        id: table

        titleCover_width : VLCStyle.listAlbumCover_width
        titleCover_height: VLCStyle.listAlbumCover_height
        titleCover_radius: VLCStyle.listAlbumCover_radius

        showTitleText: false

        //-----------------------------------------------------------------------------------------
        // TableColumns implementation

        function titlecoverLabels(model) {
            return [
                (model) ? model.resolution_name : "",
                (model) ? model.channel         : ""
            ].filter(function(a) { return a !== "" })
        }
    }

    Rectangle {
        id: line

        anchors.left : parent.left
        anchors.right: parent.right

        height: VLCStyle.dp(1)

        visible: false

        color: VLCStyle.colors.accent
    }
}
