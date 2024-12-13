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

import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

Widgets.GridItem {
    property var model: ({})
    property int index: -1

    image: model.cover || ""
    fallbackImage: VLCStyle.noArtAlbumCover

    // NOTE: If radius is 0.0, `QQuickImage` is used, and it
    //       requires a clip node (`clip: true`) to display
    //       `PreserveAspectCrop` where we can not have in a
    //       delegate. So instead, use `PreserveAspectFit` in
    //       that case. If non-RHI scene graph adaptation is
    //       used, a clip node is not a concern, so in that
    //       case `PreserveAspectCrop` can be used as well.
    fillMode: ((GraphicsInfo.shaderType !== GraphicsInfo.RhiShader) || (effectiveRadius > 0.0)) ? Image.PreserveAspectCrop
                                                                                                : Image.PreserveAspectFit

    title: model.title || qsTr("Unknown title")
    subtitle: model.main_artist || qsTr("Unknown artist")
    pictureWidth: VLCStyle.gridCover_music_width
    pictureHeight: VLCStyle.gridCover_music_height
    onPlayClicked: {
        if ( model.id !== undefined ) {
            MediaLib.addAndPlay( model.id )
        }
    }
}
