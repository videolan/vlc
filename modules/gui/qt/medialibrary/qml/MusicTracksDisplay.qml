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
import org.videolan.medialib 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface

Widgets.NavigableFocusScope {
    id: root
    property alias sortModel: tracklistdisplay_id.sortModel
    property alias model: tracklistdisplay_id.model
    property alias selectionModel: tracklistdisplay_id.selectionDelegateModel
    readonly property bool isViewMultiView: false

    Widgets.DragItem {
        id: trackDragItem

        function updateComponents(maxCovers) {
          var items = selectionModel.selectedIndexes.slice(0, maxCovers).map(function (x){
            return model.getDataAt(x.row)
          })
          var title = items.map(function (item){ return item.title}).join(", ")
          var covers = items.map(function (item) { return {artwork: item.cover || VLCStyle.noArtCover}})
          return {
            covers: covers,
            title: title,
            count: selectionModel.selectedIndexes.length
          }
        }

        function insertIntoPlaylist(index) {
            medialib.insertIntoPlaylist(index, model.getIdsForIndexes(selectionModel.selectedIndexes))
        }
    }

    MusicTrackListDisplay {
        id: tracklistdisplay_id
        anchors.fill: parent
        visible: model.count > 0
        focus: visible
        dragItem: trackDragItem
        headerTopPadding: VLCStyle.margin_normal
        navigationParent: root
        navigationCancel: function() {
            if (tracklistdisplay_id.currentIndex <= 0)
                defaultNavigationCancel()
            else
                tracklistdisplay_id.currentIndex = 0;
        }
        footer: MainInterface.MiniPlayerBottomMargin {
        }
    }

    EmptyLabel {
        anchors.fill: parent
        visible: tracklistdisplay_id.model.count === 0
        focus: visible
        text: i18n.qtr("No tracks found\nPlease try adding sources, by going to the Network tab")
        navigationParent: root
        cover: VLCStyle.noArtAlbumCover
    }
}
