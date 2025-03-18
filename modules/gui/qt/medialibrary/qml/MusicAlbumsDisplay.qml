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
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets

Widgets.PageExt {
    id: root

    title: qsTr("Albums")

    MusicAlbums {
        anchors.fill: parent

        searchPattern: MainCtx.search.pattern
        sortCriteria: MainCtx.sort.criteria
        sortOrder: MainCtx.sort.order


        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd

        enableBeginningFade: root.enableBeginningFade
        enableEndFade: root.enableEndFade

        onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
    }

}
