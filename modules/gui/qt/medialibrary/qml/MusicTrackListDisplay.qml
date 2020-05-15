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
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(1), text: i18n.qtr("Title"),    showSection: "title", colDelegate: titleDelegate, headerDelegate: titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(1), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(1), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "duration",    width: VLCStyle.colWidth(1), text: i18n.qtr("Duration"), showSection: "" },
    ]

    property var sortModelMedium: [
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(2), text: i18n.qtr("Title"),    showSection: "title", colDelegate: titleDelegate, headerDelegate: titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(2), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(1), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "duration",    width: VLCStyle.colWidth(1), text: i18n.qtr("Duration"), showSection: "" },
    ]

    property var sortModelLarge: [
        { isPrimary: true, criteria: "title",       width: VLCStyle.colWidth(2), text: i18n.qtr("Title"),    showSection: "title", colDelegate: titleDelegate, headerDelegate: titleHeaderDelegate },
        { criteria: "album_title", width: VLCStyle.colWidth(2), text: i18n.qtr("Album"),    showSection: "album_title" },
        { criteria: "main_artist", width: VLCStyle.colWidth(2), text: i18n.qtr("Artist"),   showSection: "main_artist" },
        { criteria: "duration",    width: VLCStyle.colWidth(1), text: i18n.qtr("Duration"), showSection: "" },
        { criteria: "track_number",width: VLCStyle.colWidth(1), text: i18n.qtr("Track"), showSection: "" },
        { criteria: "disc_number", width: VLCStyle.colWidth(1), text: i18n.qtr("Disc"),  showSection: "" },
    ]

    property Component titleDelegate: RowLayout {
        property var rowModel: parent.rowModel
        property var model: parent.colModel

        anchors.fill: parent
        spacing: VLCStyle.margin_normal

        Image {
            source: !rowModel ? VLCStyle.noArtCover : (rowModel.cover || VLCStyle.noArtCover)
            mipmap: true // this widget can down scale the source a lot, so for better visuals we use mipmap

            Layout.preferredHeight: VLCStyle.heightAlbumCover_xsmall
            Layout.preferredWidth: VLCStyle.heightAlbumCover_xsmall
        }

        Widgets.ListLabel {
            text: !rowModel ? "" : (rowModel[model.criteria] || "")

            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }

    property Component titleHeaderDelegate: Row {
        spacing: VLCStyle.margin_normal

        Widgets.IconLabel {
            width: VLCStyle.heightAlbumCover_xsmall
            horizontalAlignment: Text.AlignHCenter
            text: VLCIcons.album_cover
            color: VLCStyle.colors.caption
        }

        Widgets.CaptionLabel {
            text: model.text || ""
        }
    }

    sortModel: ( width < VLCStyle.colWidth(6) ) ? sortModelSmall
                                                : ( width < VLCStyle.colWidth(9) ) ? sortModelMedium : sortModelLarge
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
