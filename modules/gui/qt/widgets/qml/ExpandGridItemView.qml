/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Leon Vitanos <leon.vitanos@gmail.com>
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

import VLC.Util as Util
import VLC.Widgets as Widgets
import VLC.Style

Widgets.ExpandGridView {
    id: root

    // Properties

    // maximum picture sizing
    readonly property int maxPictureWidth: gridHelper.maxPictureWidth
    readonly property int maxPictureHeight: gridHelper.maxPictureHeight

    // constant base picture sizings
    required property int basePictureWidth
    required property int basePictureHeight

    // Aliases

    property alias titleTopMargin: gridHelper.titleTopMargin
    property alias titleHeight: gridHelper.titleHeight

    property alias subtitleTopMargin: gridHelper.subtitleTopMargin
    property alias subtitleHeight: gridHelper.subtitleHeight

    property alias maxNbItemPerRow: gridHelper.maxNbItemPerRow

    // Settings

    nbItemPerRow: gridHelper.nbItemPerRow

    // responsive cell sizing based on available area
    cellWidth: gridHelper.cellWidth
    cellHeight: gridHelper.cellHeight

    horizontalSpacing: gridHelper.horizontalSpacing

    // Children

    Util.GridSizeHelper {
        id: gridHelper

        basePictureWidth: root.basePictureWidth
        basePictureHeight: root.basePictureHeight

        availableWidth: root._availableContentWidth

        maxNbItemPerRow: basePictureWidth === basePictureHeight ? 10 : 6
    }
}
