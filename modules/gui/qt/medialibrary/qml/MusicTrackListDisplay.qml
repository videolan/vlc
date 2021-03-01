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

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.KeyNavigableTableView {
    id: root

    property var sortModelSmall: [
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(1), text: i18n.qtr("Title"),    showSection: "title", colDelegate: tableColumns.titleDelegate, headerDelegate: tableColumns.titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(1), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(1), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "duration", width: VLCStyle.colWidth(1), text: i18n.qtr("Duration"), showSection: "", colDelegate: tableColumns.timeColDelegate, headerDelegate: tableColumns.timeHeaderDelegate },
    ]

    property var sortModelMedium: [
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(2), text: i18n.qtr("Title"),    showSection: "title", colDelegate: tableColumns.titleDelegate, headerDelegate: tableColumns.titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(2), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(1), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "duration", width: VLCStyle.colWidth(1), text: i18n.qtr("Duration"), showSection: "", colDelegate: tableColumns.timeColDelegate, headerDelegate: tableColumns.timeHeaderDelegate },
    ]

    readonly property int _expandingColsSpan: Math.floor((VLCStyle.gridColumnsForWidth(root.availableRowWidth) - 3 /* static cols (track_number, etc)*/) / 3)
    property var sortModelLarge: [
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(root._expandingColsSpan), text: i18n.qtr("Title"),    showSection: "title", colDelegate: tableColumns.titleDelegate, headerDelegate: tableColumns.titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(root._expandingColsSpan), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(root._expandingColsSpan), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "duration", width: VLCStyle.colWidth(1), text: i18n.qtr("Duration"), showSection: "", colDelegate: tableColumns.timeColDelegate, headerDelegate: tableColumns.timeHeaderDelegate },
        { criteria: "track_number",width: VLCStyle.colWidth(1), text: i18n.qtr("Track"), showSection: "" },
        { criteria: "disc_number", width: VLCStyle.colWidth(1), text: i18n.qtr("Disc"),  showSection: "" },
    ]

    sortModel: ( availableRowWidth < VLCStyle.colWidth(6) ) ? sortModelSmall
                                                            : ( availableRowWidth < VLCStyle.colWidth(9) )
                                                              ? sortModelMedium : sortModelLarge
    section.property: "title_first_symbol"

    headerColor: VLCStyle.colors.bg

    model: rootmodel
    selectionDelegateModel: selectionModel
    rowHeight: VLCStyle.tableCoverRow_height

    property alias parentId: rootmodel.parentId

    onActionForSelection:  medialib.addAndPlay(model.getIdsForIndexes( selection ))
    onItemDoubleClicked: medialib.addAndPlay(model.id)
    onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, menuParent.mapToGlobal(0,0))
    onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)


    Widgets.TableColumns {
        id: tableColumns
    }

    MLAlbumTrackModel {
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

    Util.SelectableDelegateModel {
        id: selectionModel

        model: rootmodel
    }

    AlbumTrackContextMenu {
        id: contextMenu
        model: rootmodel
    }
}
