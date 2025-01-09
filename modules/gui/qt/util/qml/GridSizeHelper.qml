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

import VLC.Style

QtObject{
    // NOTE: Base picture sizing
    required property int basePictureWidth
    required property int basePictureHeight

    required property int availableWidth

    property int titleTopMargin: VLCStyle.gridItemTitle_topMargin
    property int titleHeight: VLCStyle.gridItemTitle_height

    property int subtitleTopMargin: VLCStyle.gridItemSubtitle_topMargin
    property int subtitleHeight: VLCStyle.gridItemSubtitle_height

    // NOTE: The total text height of title and subtitle
    //       1dp is added to each, to correctly represent the actual height
    readonly property int textHeight: (titleHeight ? titleHeight + titleTopMargin + VLCStyle.dp(1): 0) +
                                      (subtitleHeight ? subtitleHeight + subtitleTopMargin + VLCStyle.dp(1) : 0)

    property int horizontalSpacing: nbItemPerRow === 2 ? VLCStyle.margin_large + VLCStyle.margin_xxxsmall
                                                       : (nbItemPerRow === 3 ? VLCStyle.margin_large + VLCStyle.margin_xxsmall
                                                                             : _defaultHorizontalSpacing)

    readonly property int _defaultHorizontalSpacing: VLCStyle.column_spacing

    property var maxNbItemPerRow
    readonly property int nbItemPerRow: {
        const _nbItemPerRow = Math.max(
            Math.floor(
                (availableWidth + _defaultHorizontalSpacing) /
                (basePictureWidth + _defaultHorizontalSpacing)
            ), 1
        )

        return maxNbItemPerRow ? Math.min(_nbItemPerRow, maxNbItemPerRow) : _nbItemPerRow
    }

    // NOTE: Responsive cell sizing based on available width
    readonly property int cellWidth: (availableWidth + horizontalSpacing) / nbItemPerRow - horizontalSpacing
    readonly property int cellHeight: (basePictureHeight / basePictureWidth) * cellWidth + textHeight

    // NOTE: Find the maximum picture size for nbItemPerRow == 1, so that we downscale in most of the cases
    //       formula for maxPictureWidth depended on nbItemPerRow would be:
    //       (basePictureWidth + _defaultHorizontalSpacing) * (1 + 1 / nbItemPerRow) - _defaultHorizontalSpacing
    readonly property int maxPictureWidth: (basePictureWidth + _defaultHorizontalSpacing) * (1 + 1 / 3) - _defaultHorizontalSpacing
    readonly property int maxPictureHeight: (basePictureHeight / basePictureWidth) * maxPictureWidth
}
