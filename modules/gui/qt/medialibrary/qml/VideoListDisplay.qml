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

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///util" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"

MainInterface.MainTableView {
    id: listView_id

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    // NOTE: This is useful for groups because our main criteria is 'name' instead of 'title'.
    property string mainCriteria: "title"

    property var coverLabels

    //---------------------------------------------------------------------------------------------
    // Private

    readonly property int _nbCols: VLCStyle.gridColumnsForWidth(availableRowWidth)

    property var _modelSmall: [{
        size: Math.max(2, _nbCols),

        model: ({
            criteria: mainCriteria,

            subCriterias: [ "duration" ],

            showSection: "title",

            text: qsTr("Title"),

            placeHolder: VLCStyle.noArtVideoCover,

            headerDelegate: tableColumns.titleHeaderDelegate,
            colDelegate   : tableColumns.titleDelegate
        })
    }]

    property var _modelMedium: [{
        size: 1,

        model: ({
            type: "image",

            criteria: "thumbnail",

            text: qsTr("Cover"),

            isSortable: false,

            showSection: "",

            placeHolder: VLCStyle.noArtVideoCover,

            headerDelegate: tableColumns.titleHeaderDelegate,
            colDelegate   : tableColumns.titleDelegate
        })
    }, {
        size: Math.max(1, _nbCols - 2),

        model: ({
            criteria: mainCriteria,

            showSection: "title",

            text: qsTr("Title")
        })
    }, {
        size: 1,

        model: ({
            criteria: "duration",

            text: qsTr("Duration"),

            showSection: "",
            showContextButton: true,

            headerDelegate: tableColumns.timeHeaderDelegate,
            colDelegate   : tableColumns.timeColDelegate
        })
    }]

    // Settings

    sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                          : _modelMedium

    section.property: "title_first_symbol"

    rowHeight: VLCStyle.tableCoverRow_height

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Widgets.TableColumns {
        id: tableColumns

        showTitleText: (listView_id.sortModel === listView_id._modelSmall)
        showCriterias: showTitleText

        criteriaCover: "thumbnail"

        titleCover_height: VLCStyle.listAlbumCover_height
        titleCover_width: VLCStyle.listAlbumCover_width
        titleCover_radius: VLCStyle.listAlbumCover_radius

        titlecoverLabels: listView_id.coverLabels
    }
}
