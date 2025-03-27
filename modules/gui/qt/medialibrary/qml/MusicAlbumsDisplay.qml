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

import QtQuick


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets

MusicAlbums {
    id: root

    header: Widgets.ViewHeader {
        view: root

        visible: view.count > 0

        text: qsTr("Albums")
    }

    searchPattern: MainCtx.search.pattern
    sortCriteria: MainCtx.sort.criteria
    sortOrder: MainCtx.sort.order

    onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
}
