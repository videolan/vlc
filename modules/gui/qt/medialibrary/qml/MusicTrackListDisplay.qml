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

import VLC.MediaLibrary
import VLC.MainInterface
import VLC.Util
import VLC.Widgets as Widgets
import VLC.Style

Widgets.TableViewExt {
    id: root

    // Properties

    readonly property bool isSearchable: true

    property var pagePrefix: []

    property alias searchPattern: rootmodel.searchPattern
    property alias sortOrder: rootmodel.sortOrder
    property alias sortCriteria: rootmodel.sortCriteria
    property alias parentId: rootmodel.parentId

    // Private
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
        weight: 1,

        model: _lineTitle
    }, {
        weight: 1,

        model: _lineAlbum
    }, {
        weight: 1,

        model: _lineArtist
    }, {
        size: 1,

        model: _lineTrack
    }, {
        size: 1,

        model: _lineDisc
    }, {
        size: 1,

        model: _lineDuration
    }]

    property var _modelMedium: [{
        weight: 1,

        model: _lineTitle
    }, {
        weight: 1,

        model: _lineAlbum
    }, {
        weight: 1,

        model: _lineArtist
    }, {
        size: 1,

        model: _lineDuration
    }]

    property var _modelSmall: [{
        weight: 1,

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

    rowContextMenu: contextMenu

    onActionForSelection: (selection) => model.addAndPlay(selection)
    onItemDoubleClicked: (index, model) => MediaLib.addAndPlay(model.id)
    onRightClick: (_,_, globalMousePos) => {
        contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
    }

    Widgets.MLDragItem {
        id: tableDragItem

        indexes: indexesFlat ? root.selectionModel.selectedIndexesFlat
                             : root.selectionModel.selectedIndexes
        indexesFlat: !!root.selectionModel.selectedIndexesFlat

        view: root
    }

    Widgets.MLTableColumns {
        id: tableColumns

        showCriterias: (root.sortModel === root._modelSmall)
    }

    MLAudioModel {
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

        onLoadingChanged: {
            if (loading) {
                MainCtx.setCursor(root, Qt.BusyCursor)
                visibilityTimer.start()
            } else {
                visibilityTimer.stop()
                progressIndicator.visible = false
                MainCtx.unsetCursor(root)
            }
        }

        Component.onCompleted: {
            loadingChanged() // in case boolean default value is `true`, currently it is not
        }
    }

    MLContextMenu {
        id: contextMenu

        model: rootmodel
    }

    Widgets.ProgressIndicator {
        id: progressIndicator

        anchors.centerIn: parent
        z: 99

        visible: false

        text: ""

        Timer {
            id: visibilityTimer

            interval: VLCStyle.duration_humanMoment

            onTriggered: {
                progressIndicator.visible = true
            }
        }
    }
}
