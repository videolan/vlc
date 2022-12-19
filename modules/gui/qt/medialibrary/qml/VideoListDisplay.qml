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

    //---------------------------------------------------------------------------------------------
    // Private

    readonly property int _nbCols: VLCStyle.gridColumnsForWidth(listView_id.availableRowWidth)

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    sortModel: [{
        size: 1,

        model: ({
            type: "image",

            criteria: "thumbnail",

            showSection: "",

            placeHolder: VLCStyle.noArtVideoCover,

            headerDelegate: tableColumns.titleHeaderDelegate,
            colDelegate   : tableColumns.titleDelegate
        })
    }, {
        size: Math.max(listView_id._nbCols - 2, 1),

        model: ({
            criteria: mainCriteria,

            showSection: "title",

            text: I18n.qtr("Title")
        })
    }, {
        size: 1,

        model: ({
            criteria: "duration",

            showSection: "",
            showContextButton: true,

            headerDelegate: tableColumns.timeHeaderDelegate,
            colDelegate   : tableColumns.timeColDelegate
        })
    }]

    section.property: "title_first_symbol"

    rowHeight: VLCStyle.tableCoverRow_height

    headerColor: VLCStyle.colors.bg

    //---------------------------------------------------------------------------------------------
    // Connections
    //---------------------------------------------------------------------------------------------

    Connections {
        target: model
        onSortCriteriaChanged: {
            switch (model.sortCriteria) {
            case "title":
                listView_id.section.property = "title_first_symbol"
                break;
            default:
                listView_id.section.property = ""
            }
        }
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------
    // Events

    function onLabels(model)
    {
        if (model === null)
            return [];

        return [
            model.resolution_name || "",
            model.channel         || ""
        ].filter(function(a) { return a !== "" });
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Widgets.TableColumns {
        id: tableColumns

        showTitleText: false
        titleCover_height: VLCStyle.listAlbumCover_height
        titleCover_width: VLCStyle.listAlbumCover_width
        titleCover_radius: VLCStyle.listAlbumCover_radius

        function titlecoverLabels(model) {
            return listView_id.onLabels(model);
        }
    }
}
