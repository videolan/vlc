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

    headerColor: VLCStyle.colors.bg

    acceptDrop: true

    sortModel: [{
        size: 1,

        model: {
            criteria: "thumbnail",

            type: "image",

            headerDelegate: table.titleHeaderDelegate,
            colDelegate   : table.titleDelegate,

            placeHolder: VLCStyle.noArtAlbumCover
        }
    }, {
        size: Math.max(columns - 2, 1),

        model: {
            criteria: "title",

            text: I18n.qtr("Title")
        }
    }, {
        size: 1,

        model: {
            criteria: "duration",

            headerDelegate: table.timeHeaderDelegate,
            colDelegate   : table.timeColDelegate
        }
    }]

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onActionForSelection: MediaLib.addAndPlay(model.getIdsForIndexes(selection))
    onItemDoubleClicked: MediaLib.addAndPlay(model.id)


    onDropEntered: root._dropUpdatePosition(drag, index, delegate, before)

    onDropUpdatePosition: root._dropUpdatePosition(drag, index, delegate, before)

    onDropExited: root.hideLine(delegate)

    onDropEvent: root.applyDrop(drop, index, delegate, before)

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

    function isDroppable(drop) {
        // NOTE: Internal drop (intra-playlist).
        return Helpers.isValidInstanceOf(drop.source, Widgets.DragItem);
    }

    function applyDrop(drop, index, delegate, before) {
        if (root.isDroppable(drop) === false) {
            root.hideLine(delegate)
            return
        }

        var item = drop.source;

        var destinationIndex = before ? index : (index + 1)

        // NOTE: Move implementation.
        if (dragItem === item) {
            model.move(modelSelect.selectedIndexes, destinationIndex)

        // NOTE: Dropping medialibrary content into the playlist.
        } else if (Helpers.isValidInstanceOf(item, Widgets.DragItem)) {
            item.getSelectedInputItem(function(inputItems) {
                model.insert(inputItems, destinationIndex)
            })
        }

        root.forceActiveFocus()

        root.hideLine(delegate)
    }

    function _dropUpdatePosition(drag, index, delegate, before) {
        if (root.isDroppable(drag) === false) {
            root.hideLine(delegate)
            return
        }

        root.showLine(delegate, before)
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
    }

    function hideLine(item)
    {
        // NOTE: We want to make sure we're not being called after the 'showLine' function.
        if (_item !== item)
            return;

        _item = null;
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

        visible: root._item !== null

        color: VLCStyle.colors.accent
    }
}
