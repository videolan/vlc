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

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/"    as MainInterface
import "qrc:///style/"

MainInterface.MainTableView {
    id: playlistDisplay

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    readonly property int columns: VLCStyle.gridColumnsForWidth(playlistDisplay.availableRowWidth)

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    rowHeight: VLCStyle.tableCoverRow_height

    headerColor: VLCStyle.colors.bg

    sortModel: [{
        criteria: "thumbnail",

        width: VLCStyle.colWidth(1),

        type: "image",

        headerDelegate: table.titleHeaderDelegate,
        colDelegate   : table.titleDelegate
    }, {
        isPrimary: true,

        criteria: "title",

        width: VLCStyle.colWidth(Math.max(columns - 2, 1)),

        text: i18n.qtr("Title")
    }, {
        criteria: "duration_short",

        width: VLCStyle.colWidth(1),

        headerDelegate: table.timeHeaderDelegate,
        colDelegate   : table.timeColDelegate
    }]

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onActionForSelection: medialib.addAndPlay(model.getIdsForIndexes(selection))

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Widgets.TableColumns {
        id: table

        titleCover_width : VLCStyle.listAlbumCover_width
        titleCover_height: VLCStyle.listAlbumCover_height
        titleCover_radius: VLCStyle.listAlbumCover_radius

        showTitleText: false

        function titlecoverLabels(model) {
            return [
                (model) ? model.resolution_name : "",
                (model) ? model.channel         : ""
            ].filter(function(a) { return a !== "" })
        }
    }
}
