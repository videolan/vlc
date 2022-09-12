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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.GridItem {
    id: root

    width: VLCStyle.gridItem_network_width
    height: VLCStyle.gridItem_network_height

    pictureWidth: VLCStyle.gridCover_network_width
    pictureHeight: VLCStyle.gridCover_network_height

    playCoverBorderWidth: VLCStyle.gridCover_network_border

    playCoverShowPlay: (model.type !== NetworkMediaModel.TYPE_NODE
                        &&
                        model.type !== NetworkMediaModel.TYPE_DIRECTORY)

    image: {
        if (model.artwork && model.artwork.toString() !== "") {
            return model.artwork
        } else {
            var f = function(type) {
                switch (type) {
                case NetworkMediaModel.TYPE_DISC:
                    return "qrc://sd/disc.svg"
                case NetworkMediaModel.TYPE_CARD:
                    return "qrc://sd/capture-card.svg"
                case NetworkMediaModel.TYPE_STREAM:
                    return "qrc://sd/stream.svg"
                case NetworkMediaModel.TYPE_PLAYLIST:
                    return "qrc://sd/playlist.svg"
                case NetworkMediaModel.TYPE_FILE:
                    return "qrc://sd/file.svg"
                default:
                    return "qrc://sd/directory.svg"
                }
            }
            return SVGColorImage.colorize(f(model.type))
                                .color1(VLCStyle.colors.text)
                                .accent(VLCStyle.colors.accent)
                                .uri()
        }
    }

    title: model.name || I18n.qtr("Unknown share")
    subtitle: {
       if (!model.mrl) {
         return ""
       } else if ((model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY) && model.mrl.toString() === "vlc://nop") {
         return ""
      } else {
         return model.mrl
      }
    }
}
