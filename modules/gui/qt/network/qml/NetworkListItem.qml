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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.ListItem {
    id: item

    width: root.width
    height: VLCStyle.icon_normal + VLCStyle.margin_small

    focus: true

    cover: Image {
        id: cover_obj
        fillMode: Image.PreserveAspectFit
        source: {
            if (model.artwork && model.artwork.toString() !== "") {
                return model.artwork
            }

            switch (model.type) {
            case NetworkMediaModel.TYPE_DISC:
                return  "qrc:///sd/disc.svg"
            case NetworkMediaModel.TYPE_CARD:
                return  "qrc:///sd/capture-card.svg"
            case NetworkMediaModel.TYPE_STREAM:
                return  "qrc:///sd/stream.svg"
            case NetworkMediaModel.TYPE_PLAYLIST:
                return  "qrc:///sd/playlist.svg"
            case NetworkMediaModel.TYPE_FILE:
                return  "qrc:///sd/file.svg"
            default:
                return "qrc:///sd/directory.svg"
            }
        }
    }
    line1: model.name || I18n.qtr("Unknown share")
    line2: model.mrl
    imageText: (model.type !== NetworkMediaModel.TYPE_DIRECTORY && model.type !== NetworkMediaModel.TYPE_NODE) ? model.protocol : ""

    showContextButton: true

    actionButtons: []
}
