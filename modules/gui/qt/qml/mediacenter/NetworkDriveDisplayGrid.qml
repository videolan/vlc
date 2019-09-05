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

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.GridItem {
    id: item
    function getImage(type){
        switch (type){

        case MLNetworkModel.TYPE_DISC:
            return  "qrc:///type/disc.svg"
        case MLNetworkModel.TYPE_CARD:
            return  "qrc:///type/capture-card.svg"
        case MLNetworkModel.TYPE_STREAM:
            return  "qrc:///type/stream.svg"
        case MLNetworkModel.TYPE_PLAYLIST:
            return  "qrc:///type/playlist.svg"

        default:
            return "qrc:///type/directory_black.svg"
        }
    }
    pictureWidth: VLCStyle.network_normal
    pictureHeight: VLCStyle.network_normal
    image: item.getImage(model.type)
    subtitle: model.mrl
    title: model.name || qsTr("Unknown share")
    focus: true
    onItemClicked : {
        if (key == Qt.RightButton){
            contextMenu.model = model
            contextMenu.popup(menuParent)
        }
        delegateModel.updateSelection( modifier ,  view[viewIndexPropertyName], index)
        view[viewIndexPropertyName] = index
        item.forceActiveFocus()
    }
    noActionButtons: true
    showContextButton: true
    
    onAddToPlaylistClicked: model.indexed = !model.indexed
    onContextMenuButtonClicked: {
        contextMenu.model = model
        contextMenu.popup(menuParent)
    }

    onItemDoubleClicked: {
        history.push( ["mc", "network", { tree: model.tree } ], History.Go)
    }
}
