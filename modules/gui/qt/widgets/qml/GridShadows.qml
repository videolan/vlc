/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import "qrc:///style/"

Item {
    id: root

    property alias leftPadding: unselectedShadow.leftPadding
    property alias topPadding: unselectedShadow.topPadding
    property alias coverMargins: unselectedShadow.coverMargins
    property alias coverWidth: unselectedShadow.coverWidth
    property alias coverHeight: unselectedShadow.coverHeight
    property alias coverRadius: unselectedShadow.coverRadius

    property alias unselected: unselectedShadow.imageComponent
    property alias selected: selectedShadow.imageComponent

    ShadowCoverGenerator {
        id: unselectedShadow

        leftPadding: 0
        topPadding: 0
        coverMargins: 1
        coverWidth: VLCStyle.colWidth(1)
        coverHeight: VLCStyle.colWidth(1)
        coverRadius: VLCStyle.gridCover_radius

        secondaryVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)
        secondaryRadius: VLCStyle.dp(2, VLCStyle.scale)
        secondarySamples: 1 + VLCStyle.dp(2, VLCStyle.scale) * 2
        primaryVerticalOffset: VLCStyle.dp(4, VLCStyle.scale)
        primaryRadius: VLCStyle.dp(9, VLCStyle.scale)
        primarySamples: 1 + VLCStyle.dp(9, VLCStyle.scale) * 2
    }

    ShadowCoverGenerator {
        id: selectedShadow

        leftPadding: root.leftPadding
        topPadding: root.topPadding
        coverMargins: root.coverMargins
        coverWidth: root.coverWidth
        coverHeight: root.coverHeight
        coverRadius: root.coverRadius

        secondaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
        secondaryRadius: VLCStyle.dp(18, VLCStyle.scale)
        secondarySamples: 1 + VLCStyle.dp(18, VLCStyle.scale) * 2
        primaryVerticalOffset: VLCStyle.dp(32, VLCStyle.scale)
        primaryRadius: VLCStyle.dp(72, VLCStyle.scale)
        primarySamples: 1 + VLCStyle.dp(72, VLCStyle.scale) * 2
    }
}
