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
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.KeyNavigableTableView {
    id: root

    property var sortModelSmall: [
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(1), text: i18n.qtr("Title"),    showSection: "title", colDelegate: tableColumns.titleDelegate, headerDelegate: tableColumns.titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(1), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(1), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "durationShort", width: VLCStyle.colWidth(1), showSection: "", colDelegate: tableColumns.timeColDelegate, headerDelegate: tableColumns.timeHeaderDelegate },
    ]

    property var sortModelMedium: [
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(2), text: i18n.qtr("Title"),    showSection: "title", colDelegate: tableColumns.titleDelegate, headerDelegate: tableColumns.titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(2), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(1), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "durationShort", width: VLCStyle.colWidth(1), showSection: "", colDelegate: tableColumns.timeColDelegate, headerDelegate: tableColumns.timeHeaderDelegate },
    ]

    property var sortModelLarge: [
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(2), text: i18n.qtr("Title"),    showSection: "title", colDelegate: tableColumns.titleDelegate, headerDelegate: tableColumns.titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(2), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(2), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "durationShort", width: VLCStyle.colWidth(1), showSection: "", colDelegate: tableColumns.timeColDelegate, headerDelegate: tableColumns.timeHeaderDelegate },
        { criteria: "track_number",width: VLCStyle.colWidth(1), text: i18n.qtr("Track"), showSection: "" },
        { criteria: "disc_number", width: VLCStyle.colWidth(1), text: i18n.qtr("Disc"),  showSection: "" },
    ]

    sortModel: ( availableRowWidth < VLCStyle.colWidth(6) ) ? sortModelSmall
                                                            : ( availableRowWidth < VLCStyle.colWidth(9) )
                                                              ? sortModelMedium : sortModelLarge
    section.property: "title_first_symbol"

    headerColor: VLCStyle.colors.bg

    model: MLAlbumTrackModel {
        id: rootmodel
        ml: medialib
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

    property alias parentId: rootmodel.parentId

    onActionForSelection:  medialib.addAndPlay(model.getIdsForIndexes( selection ))

    Widgets.TableColumns {
        id: tableColumns
    }
}
