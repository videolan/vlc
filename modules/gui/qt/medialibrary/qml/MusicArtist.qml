/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Controls 2.4
import QtQuick 2.11
import QtQml.Models 2.2
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

MusicAlbums {
    id: root

    property var artist: ({})

    gridViewMarginTop: 0
    focus: true
    navigationUpItem: headerItem

    header: ArtistTopBanner {
        id: artistBanner

        width: root.width
        artist: root.artist
        navigationParent: root.navigationParent
        navigationLeftItem: root.navigationLeftItem
        navigationDown: function() {
            artistBanner.focus = false
            root.forceActiveFocus()
        }
    }
}
