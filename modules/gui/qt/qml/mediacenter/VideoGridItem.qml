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

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.GridItem {
    property var model: ({})
    image: model.thumbnail || VLCStyle.noArtCover
    title: model.title || qsTr("Unknown title")
    infoLeft: model.duration || ""
    resolution: model.resolution_name || ""
    channel: model.channel || ""
    isVideo: true
    isNew: model.playcount < 1
    showContextButton: true
    progress: model.saved_position > 0 ? model.saved_position : 0
    pictureWidth: VLCStyle.video_normal_width
    pictureHeight: VLCStyle.video_normal_height
    onItemDoubleClicked: if ( model.id !== undefined ) { medialib.addAndPlay( model.id ) }
}
