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

import QtQuick
import QtQuick.Controls
import QtQml.Models

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

    property bool isMusic

    //---------------------------------------------------------------------------------------------
    // Private

    property Item _item: null

    property bool _before: true

    property var _modelSmall: [{
        size: Math.max(2, columns),

        model: {
            criteria: "title",

            subCriterias: [ "duration" ],

            text: qsTr("Title"),

            headerDelegate: table.titleHeaderDelegate,
            colDelegate   : table.titleDelegate,

            placeHolder: VLCStyle.noArtAlbumCover
        }
    }]

    property var _modelMedium: [{
        size: 1,

        model: {
            criteria: "thumbnail",

            text: qsTr("Cover"),

            type: "image",

            headerDelegate: table.titleHeaderDelegate,
            colDelegate   : table.titleDelegate,

            placeHolder: VLCStyle.noArtAlbumCover
        }
    }, {
        size: Math.max(1, columns - 2),

        model: {
            criteria: "title",

            text: qsTr("Title")
        }
    }, {
        size: 1,

        model: {
            criteria: "duration",

            text: qsTr("Duration"),

            headerDelegate: table.timeHeaderDelegate,
            colDelegate   : table.timeColDelegate
        }
    }]

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    rowHeight: VLCStyle.tableCoverRow_height

    acceptDrop: true

    sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                          : _modelMedium

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onActionForSelection: model.addAndPlay( selection )
    onItemDoubleClicked: MediaLib.addAndPlay(model.id)


    onDropEntered: (delegate, index, drag, before) => {
        root._dropUpdatePosition(drag, index, delegate, before)
    }

    onDropUpdatePosition: (delegate, index, drag, before) => {
        root._dropUpdatePosition(drag, index, delegate, before)
    }

    onDropExited: (delegate, index, drag, before) => {
        root.hideLine(delegate)
    }

    onDropEvent: (delegate, index, drag, drop, before) => {
        root.applyDrop(drop, index, delegate, before)
    }

    //---------------------------------------------------------------------------------------------
    // Connections
    //---------------------------------------------------------------------------------------------

    Connections {
        target: root

        // NOTE: We want to hide the drop line when scrolling so its position stays relevant.
        function onContentYChanged() {
            hideLine(_item)
        }
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------
    // Drop interface

    function isDroppable(drop, index) {
        if (drop.source === dragItem) {
            return Helpers.itemsMovable(selectionModel.sortedSelectedIndexesFlat, index)
        } else if (Helpers.isValidInstanceOf(drop.source, Widgets.DragItem)) {
            return true
        } else if (drop.hasUrls) {
            return true
        } else {
            return false
        }
    }

    function applyDrop(drop, index, delegate, before) {
        if (root.isDroppable(drop, index + (before ? 0 : 1)) === false) {
            root.hideLine(delegate)
            return
        }

        const item = drop.source;

        const destinationIndex = before ? index : (index + 1)

        // NOTE: Move implementation.
        if (dragItem === item) {
            model.move(selectionModel.selectedRows(), destinationIndex)
        // NOTE: Dropping medialibrary content into the playlist.
        } else if (Helpers.isValidInstanceOf(item, Widgets.DragItem)) {
            item.getSelectedInputItem()
                .then(inputItems => {
                    model.insert(inputItems, destinationIndex)
                })
        } else if (drop.hasUrls) {
            const urlList = []
            for (let url in drop.urls)
                urlList.push(drop.urls[url])

            model.insert(urlList, destinationIndex)
        }

        root.forceActiveFocus()

        root.hideLine(delegate)
    }

    function _dropUpdatePosition(drag, index, delegate, before) {
        if (root.isDroppable(drag, index + (before ? 0 : 1)) === false) {
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

        titleCover_width: isMusic ? VLCStyle.trackListAlbumCover_width
                                  : VLCStyle.listAlbumCover_width
        titleCover_height: isMusic ? VLCStyle.trackListAlbumCover_heigth
                                   : VLCStyle.listAlbumCover_height
        titleCover_radius: isMusic ? VLCStyle.trackListAlbumCover_radius
                                   : VLCStyle.listAlbumCover_radius

        showTitleText: (root.sortModel === root._modelSmall)
        showCriterias: showTitleText

        criteriaCover: "thumbnail"

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

        color: root.colorContext.accent
    }
}
