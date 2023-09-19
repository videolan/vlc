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
import QtQuick
import QtQuick.Controls
import QtQml.Models
import QtQuick.Layouts

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.KeyNavigableTableView {
    id: root

    // Properties

    readonly property bool isSearchable: true

    property var pagePrefix: []

    property alias searchPattern: rootmodel.searchPattern
    property alias sortOrder: rootmodel.sortOrder
    property alias sortCriteria: rootmodel.sortCriteria
    property alias parentId: rootmodel.parentId

    // Private
    property int _nbCols: VLCStyle.gridColumnsForWidth(availableRowWidth)

    readonly property int _sizeA: Math.floor((_nbCols - 3) / 3)
    readonly property int _sizeB: Math.floor((_nbCols - 2) / 2)

    property var _lineTitle: ({
        criteria: "title",

        text: qsTr("Title"),

        showSection: "title",

        colDelegate: tableColumns.titleDelegate,
        headerDelegate: tableColumns.titleHeaderDelegate,

        placeHolder: VLCStyle.noArtAlbumCover
    })

    property var _lineAlbum: ({
        criteria: "album_title",

        text: qsTr("Album"),

        showSection: "album_title"
    })

    property var _lineArtist: ({
        criteria: "main_artist",

        text: qsTr("Artist"),

        showSection: "main_artist"
    })

    property var _lineDuration: ({
        criteria: "duration",

        text: qsTr("Duration"),

        showSection: "",

        colDelegate: tableColumns.timeColDelegate,
        headerDelegate: tableColumns.timeHeaderDelegate
    })

    property var _lineTrack: ({
        criteria: "track_number",

        text: qsTr("Track"),

        showSection: ""
    })

    property var _lineDisc: ({
        criteria: "disc_number",

        text: qsTr("Disc"),

        showSection: ""
    })

    property var _modelLarge: [{
        size: _sizeA,

        model: _lineTitle
    }, {
        size: _sizeA,

        model: _lineAlbum
    }, {
        size: _sizeA,

        model: _lineArtist
    }, {
        size: 1,

        model: _lineDuration
    }, {
        size: 1,

        model: _lineTrack
    }, {
        size: 1,

        model: _lineDisc
    }]

    property var _modelMedium: [{
        size: _sizeB,

        model: _lineTitle
    }, {
        size: _sizeB,

        model: _lineAlbum
    }, {
        size: 1,

        model: _lineArtist
    }, {
        size: 1,

        model: _lineDuration
    }]

    property var _modelSmall: [{
        size: Math.max(2, _nbCols),

        model: ({
            criteria: "title",

            subCriterias: [ "duration", "album_title" ],

            text: qsTr("Title"),

            showSection: "title",

            colDelegate: tableColumns.titleDelegate,
            headerDelegate: tableColumns.titleHeaderDelegate,

            placeHolder: VLCStyle.noArtAlbumCover
        })
    }]

    // Settings

    sortModel: {
        if (availableRowWidth < VLCStyle.colWidth(4))
            return _modelSmall
        else if (availableRowWidth < VLCStyle.colWidth(9))
            return _modelMedium
        else
            return _modelLarge
    }

    section.property: "title_first_symbol"

    model: rootmodel
    rowHeight: VLCStyle.tableCoverRow_height

    dragItem: tableDragItem

    onDragItemChanged: console.assert(root.dragItem === tableDragItem)

    onActionForSelection: (selection) => model.addAndPlay(selection)
    onItemDoubleClicked: (index, model) => MediaLib.addAndPlay(model.id)
    onContextMenuButtonClicked: (_,_, globalMousePos) => {
        contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
    }
    onRightClick: (_,_, globalMousePos) => {
        contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
    }

    Widgets.MLDragItem {
        id: tableDragItem

        indexes: indexesFlat ? root.selectionModel.selectedIndexesFlat
                             : root.selectionModel.selectedIndexes
        indexesFlat: !!root.selectionModel.selectedIndexesFlat

        mlModel: model
    }

    Widgets.TableColumns {
        id: tableColumns

        showCriterias: (root.sortModel === root._modelSmall)
    }

    MLAlbumTrackModel {
        id: rootmodel
        ml: MediaLib

        onSortCriteriaChanged: {
            switch (rootmodel.sortCriteria) {
            case "title":
            case "album_title":
            case "main_artist":
                section.property = rootmodel.sortCriteria + "_first_symbol"
                break;
            default:
                section.property = ""
            }
        }
    }

    Util.MLContextMenu {
        id: contextMenu

        model: rootmodel
    }
}
