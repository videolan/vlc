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

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers

Widgets.GridItem {
    property var model: ({})
    property int index: -1

    image: model.cover || ""
    fallbackImage: VLCStyle.noArtAlbumCover

    title: model.title || qsTr("Unknown title")
    subtitle: model.main_artist || qsTr("Unknown artist")
    pictureWidth: VLCStyle.gridCover_music_width
    pictureHeight: VLCStyle.gridCover_music_height
    playCoverBorderWidth: VLCStyle.gridCover_music_border
    onPlayClicked: {
        if ( model.id !== undefined ) {
            MediaLib.addAndPlay( model.id )
        }
    }
}
