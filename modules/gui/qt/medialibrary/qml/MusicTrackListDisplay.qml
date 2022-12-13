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
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.KeyNavigableTableView {
    id: root

    // Properties

    readonly property int _expandingColsSpan: Math.floor((VLCStyle.gridColumnsForWidth(root.availableRowWidth) - 3 /* static cols (track_number, etc)*/) / 3)

    property alias parentId: rootmodel.parentId

    // Private

    property var _lineTitle: ({
        criteria: "title",

        text: I18n.qtr("Title"),

        showSection: "title",

        colDelegate: tableColumns.titleDelegate,
        headerDelegate: tableColumns.titleHeaderDelegate,

        placeHolder: VLCStyle.noArtAlbumCover
    })

    property var _lineAlbum: ({
        criteria: "album_title",

        text: I18n.qtr("Album"),

        showSection: "album_title"
    })

    property var _lineArtist: ({
        criteria: "main_artist",

        text: I18n.qtr("Artist"),

        showSection: "main_artist"
    })

    property var _lineDuration: ({
        criteria: "duration",

        text: I18n.qtr("Duration"),

        showSection: "",

        colDelegate: tableColumns.timeColDelegate,
        headerDelegate: tableColumns.timeHeaderDelegate
    })

    property var _lineTrack: ({
        criteria: "track_number",

        text: I18n.qtr("Track"),

        showSection: ""
    })

    property var _lineDisc: ({
        criteria: "disc_number",

        text: I18n.qtr("Disc"),

        showSection: ""
    })

    property var _modelLarge: [{
        isPrimary: true,

        width: VLCStyle.colWidth(_expandingColsSpan),

        model: _lineTitle
    }, {
        width: VLCStyle.colWidth(_expandingColsSpan),

        model: _lineAlbum
    }, {
        width: VLCStyle.colWidth(_expandingColsSpan),

        model: _lineArtist
    }, {
        width: VLCStyle.colWidth(1),

        model: _lineDuration
    }, {
        width: VLCStyle.colWidth(1),

        model: _lineTrack
    }, {
        width: VLCStyle.colWidth(1),

        model: _lineDisc
    }]

    property var _modelMedium: [{
        isPrimary: true,

        width: VLCStyle.colWidth(2),

        model: _lineTitle
    }, {
        width: VLCStyle.colWidth(2),

        model: _lineAlbum
    }, {
        width: VLCStyle.colWidth(1),

        model: _lineArtist
    }, {
        width: VLCStyle.colWidth(1),

        model: _lineDuration
    }]

    property var _modelSmall: [{
        isPrimary: true,

        width: VLCStyle.colWidth(1),

        model: _lineTitle
    }, {
        width: VLCStyle.colWidth(1),

        model: _lineAlbum
    }, {
        width: VLCStyle.colWidth(1),

        model: _lineArtist
    }, {
        width: VLCStyle.colWidth(1),

        model: _lineDuration
    }]

    sortModel: {
        if (availableRowWidth < VLCStyle.colWidth(6))
            return _modelSmall
        else if (availableRowWidth < VLCStyle.colWidth(9))
            return _modelMedium
        else
            return _modelLarge
    }

    section.property: "title_first_symbol"

    headerColor: VLCStyle.colors.bg

    model: rootmodel
    selectionDelegateModel: selectionModel
    rowHeight: VLCStyle.tableCoverRow_height

    onActionForSelection:  MediaLib.addAndPlay(model.getIdsForIndexes( selection ))
    onItemDoubleClicked: MediaLib.addAndPlay(model.id)
    onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
    onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)

    dragItem: Widgets.MLDragItem {
        indexes: selectionModel.selectedIndexes

        mlModel: model
    }

    Widgets.TableColumns {
        id: tableColumns
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

    Util.SelectableDelegateModel {
        id: selectionModel

        model: rootmodel
    }

    Util.MLContextMenu {
        id: contextMenu

        model: rootmodel
    }
}
