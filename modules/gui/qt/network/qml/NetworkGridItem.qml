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

import VLC.Widgets as Widgets
import VLC.Network
import VLC.Style
import VLC.Util

Widgets.GridItem {
    id: root

    property var model: ({})
    property int index: -1

    pictureWidth: VLCStyle.gridCover_network_width
    pictureHeight: VLCStyle.gridCover_network_height

    playCoverShowPlay: (model.type !== NetworkMediaModel.TYPE_NODE
                        &&
                        model.type !== NetworkMediaModel.TYPE_DIRECTORY)

    image: {
        if (model.artwork && model.artwork.toString() !== "") {
            return model.artwork
        }
        return ""
    }

    fillMode: Image.PreserveAspectCrop

    cacheImage: true // we may have network thumbnail

    fallbackImage: {
        return SVGColorImage.colorize(model.artworkFallback)
                            .color1(root.colorContext.fg.primary)
                            .accent(root.colorContext.accent)
                            .uri()
    }

    title: model.name || qsTr("Unknown share")
    subtitle: {
        // make sure subtitle is never empty otherwise it causes alignment issues
        const defaultTxt = "--"

        const mrl = model.mrl?.toString() || ""
        const type = model.type

        if ((type === NetworkMediaModel.TYPE_NODE || type === NetworkMediaModel.TYPE_DIRECTORY)
            && (mrl === "vlc://nop"))
            return defaultTxt

        return mrl || defaultTxt
    }

    pictureOverlay: Item {
        implicitWidth: root.pictureWidth
        implicitHeight: root.pictureHeight

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
