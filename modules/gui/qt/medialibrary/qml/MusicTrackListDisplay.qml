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

    sortModel: [
        { isPrimary: true, criteria: "title",       width:0.44, text: i18n.qtr("Title"),    showSection: "title" },
        { criteria: "album_title", width:0.25, text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width:0.15, text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "duration",    width:0.06, text: i18n.qtr("Duration"), showSection: "" },
        { criteria: "track_number",width:0.05, text: i18n.qtr("Track"), showSection: "" },
        { criteria: "disc_number", width:0.05, text: i18n.qtr("Disc"),  showSection: "" },
    ]

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

    colDelegate: Item {
        anchors.fill: parent

        property var rowModel: parent.rowModel
        property var model: parent.colModel

        Text {
            anchors.fill:parent

            text: !rowModel ? "" : (rowModel[model.criteria] || "")
            elide: Text.ElideRight
            font.pixelSize: VLCStyle.fontSize_normal
            color: (model.isPrimary)? VLCStyle.colors.text : VLCStyle.colors.textDisabled

            anchors {
                fill: parent
                leftMargin: VLCStyle.margin_xsmall
                rightMargin: VLCStyle.margin_xsmall
            }
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
        }
    }

    onActionForSelection:  medialib.addAndPlay(model.getIdsForIndexes( selection ))
}
