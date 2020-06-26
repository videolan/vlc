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

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.KeyNavigableTableView {
    id: listView_id

    model: MLVideoModel {
        ml: medialib
    }

    property Component thumbnailHeader: Item {
        Widgets.IconLabel {
            height: VLCStyle.listAlbumCover_height
            width: VLCStyle.listAlbumCover_width
            horizontalAlignment: Text.AlignHCenter
            text: VLCIcons.album_cover
            color: VLCStyle.colors.caption
        }
    }

    property Component thumbnailColumn: Item {

        property var rowModel: parent.rowModel
        property var model: parent.colModel
        readonly property bool currentlyFocused: parent.currentlyFocused
        readonly property bool containsMouse: parent.containsMouse

        Widgets.MediaCover {
            anchors.verticalCenter: parent.verticalCenter
            source: ( !rowModel ? undefined : rowModel[model.criteria] ) || VLCStyle.noArtCover
            playCoverVisible: currentlyFocused || containsMouse
            playIconSize: VLCStyle.play_cover_small
            onPlayIconClicked:  medialib.addAndPlay( rowModel.id )
            labels: [
                !rowModel ? "" : rowModel.resolution_name,
                !rowModel ? "" : rowModel.channel
            ].filter(function(a) { return a !== "" } )
        }

    }

    readonly property int _nbCols: VLCStyle.gridColumnsForWidth(listView_id.availableRowWidth)

    sortModel: [
        { type: "image", criteria: "thumbnail", width: VLCStyle.colWidth(1), showSection: "", colDelegate: thumbnailColumn, headerDelegate: thumbnailHeader },
        { isPrimary: true, criteria: "title",   width: VLCStyle.colWidth(Math.max(listView_id._nbCols - 2, 1)), text: i18n.qtr("Title"),    showSection: "title" },
        { criteria: "durationShort",            width: VLCStyle.colWidth(1), showSection: "", colDelegate: tableColumns.timeColDelegate, headerDelegate: tableColumns.timeHeaderDelegate, showContextButton: true },
    ]

    section.property: "title_first_symbol"

    rowHeight: VLCStyle.listAlbumCover_height + VLCStyle.margin_xxsmall * 2

    headerColor: VLCStyle.colors.bg

    onActionForSelection: medialib.addAndPlay(model.getIdsForIndexes( selection ))

    navigationLeft:  function(index) {
        if (isFocusOnContextButton )
            isFocusOnContextButton = false
        else
            defaultNavigationLeft(index)
    }
    navigationRight: function(index) {
        if (!isFocusOnContextButton)
            isFocusOnContextButton = true
        else
            defaultNavigationRight(index)
    }

    Widgets.TableColumns {
        id: tableColumns
    }
}
