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

Utils.ListItem {
    id: item

    width: root.width
    height: VLCStyle.icon_normal + VLCStyle.margin_small

    focus: true

    color: VLCStyle.colors.getBgColor(element.DelegateModel.inSelected, this.hovered, this.activeFocus)

    cover: Image {
        id: cover_obj
        fillMode: Image.PreserveAspectFit
        source: {
            switch (model.type) {
            case MLNetworkMediaModel.TYPE_DISC:
                return  "qrc:///type/disc.svg"
            case MLNetworkMediaModel.TYPE_CARD:
                return  "qrc:///type/capture-card.svg"
            case MLNetworkMediaModel.TYPE_STREAM:
                return  "qrc:///type/stream.svg"
            case MLNetworkMediaModel.TYPE_PLAYLIST:
                return  "qrc:///type/playlist.svg"
            case MLNetworkMediaModel.TYPE_FILE:
                return  "qrc:///type/file_black.svg"
            default:
                return "qrc:///type/directory_black.svg"
            }
        }
    }
    line1: model.name || qsTr("Unknown share")
    line2: model.mrl
    imageText: (model.type !== MLNetworkMediaModel.TYPE_DIRECTORY && model.type !== MLNetworkMediaModel.TYPE_NODE) ? model.protocol : ""

    showContextButton: true

    actionButtons: []
}
