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

import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Util
import VLC.Style

Widgets.TableViewExt {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    property bool isMusic

    //---------------------------------------------------------------------------------------------
    // Private

    property Item _item: null

    property bool _before: true

    property var _modelSmall: [{
        weight: 1,

        model: {
            criteria: "title",

            subCriterias: [ "duration" ],

            text: qsTr("Title"),

            isSortable: false,

            headerDelegate: table.titleHeaderDelegate,
            colDelegate   : table.titleDelegate,

            placeHolder: VLCStyle.noArtAlbumCover
        }
    }]

    property var _modelMedium: [{
        weight: 1,

        model: {
            criteria: "title",

            text: qsTr("Title"),

            isSortable: false,

            headerDelegate: table.titleHeaderDelegate,
            colDelegate   : table.titleDelegate,

            placeHolder: VLCStyle.noArtAlbumCover
        }
    }, {
        size: 1,

        model: {
            criteria: "duration",

            text: qsTr("Duration"),

            isSortable: false,

            headerDelegate: table.timeHeaderDelegate,
            colDelegate   : table.timeColDelegate
        }
    }]

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    rowHeight: VLCStyle.tableCoverRow_height

    sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                          : _modelMedium


    listView.acceptDropFunc: function(index, drop) {
        // FIXME: The DnD API seems quite poorly designed in this file.
        //        Why does it ask for both index and "before"
        //        When index + 1 is essentially the same as
        //        before being false?
        //        What is "delegate" and why is it passed in applyDrop()?
        //        We have to come up with a shim function here...
        return applyDrop(drop, index - 1, null, false)
    }

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onActionForSelection: (selection) => model.addAndPlay( selection )
    onItemDoubleClicked: (index, model) => MediaLib.addAndPlay(model.id)

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------
    // Drop interface

    listView.isDropAcceptableFunc: function(drop, index) {
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
        if (listView.isDropAcceptableFunc(drop, index + (before ? 0 : 1)) === false) {
            drop.accepted = false
            return Promise.resolve()
        }

        const item = drop.source;

        const destinationIndex = before ? index : (index + 1)

        // NOTE: Move implementation.
        if (dragItem === item) {
            model.move(selectionModel.selectedRows(), destinationIndex)
            root.forceActiveFocus()
        // NOTE: Dropping medialibrary content into the playlist.
        } else if (Helpers.isValidInstanceOf(item, Widgets.DragItem)) {
            return item.getSelectedInputItem()
                        .then(inputItems => {
                            model.insert(inputItems, destinationIndex)
                        })
                        .then(() => { root.forceActiveFocus(); })
        } else if (drop.hasUrls) {
            const urlList = []
            for (let url in drop.urls)
                urlList.push(drop.urls[url])

            model.insert(urlList, destinationIndex)

            root.forceActiveFocus()
        }

        return Promise.resolve()
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Widgets.MLTableColumns {
        id: table

        titleCover_width: isMusic ? VLCStyle.trackListAlbumCover_width
                                  : VLCStyle.listAlbumCover_width
        titleCover_height: isMusic ? VLCStyle.trackListAlbumCover_heigth
                                   : VLCStyle.listAlbumCover_height
        titleCover_radius: isMusic ? VLCStyle.trackListAlbumCover_radius
                                   : VLCStyle.listAlbumCover_radius

        showCriterias: (root.sortModel === root._modelSmall)

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
