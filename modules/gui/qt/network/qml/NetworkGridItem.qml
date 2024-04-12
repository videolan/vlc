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
import QtQuick
import QtQuick.Controls
import QtQml.Models
import Qt5Compat.GraphicalEffects

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

Widgets.GridItem {
    id: root

    property var model: ({})
    property int index: -1

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
        }
        return ""
    }

    cacheImage: true // we may have network thumbnail

    fallbackImage: {
        const f = function(type) {
            switch (type) {
            case NetworkMediaModel.TYPE_DISC:
                return "qrc:///sd/disc.svg"
            case NetworkMediaModel.TYPE_CARD:
                return "qrc:///sd/capture-card.svg"
            case NetworkMediaModel.TYPE_STREAM:
                return "qrc:///sd/stream.svg"
            case NetworkMediaModel.TYPE_PLAYLIST:
                return "qrc:///sd/playlist.svg"
            case NetworkMediaModel.TYPE_FILE:
                return "qrc:///sd/file.svg"
            default:
                return "qrc:///sd/directory.svg"
            }
        }

        return SVGColorImage.colorize(f(model.type))
                            .color1(root.colorContext.fg.primary)
                            .accent(root.colorContext.accent)
                            .uri()
    }

    title: model.name || qsTr("Unknown share")
    subtitle: {
        if (!model.mrl) {
            return ""
        } else if ((model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY) && model.mrl.toString() === "vlc://nop") {
            return ""
        } else {
            return model.mrl
        }
    }

    pictureOverlay: Item {
        width: root.pictureWidth
        height: root.pictureHeight

        Widgets.VideoProgressBar {
            id: progressBar

            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }

            visible: (model.progress ?? - 1) > 0

            radius: root.pictureRadius
            value:  visible
                    ? Helpers.clamp(model.progress, 0, 1)
                    : 0
        }
    }
}
