
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
import QtQuick 2.11
import QtGraphicalEffects 1.0

import "qrc:///style/"
import "qrc:///util/KeyHelper.js" as KeyHelper

CoverShadow {
    id: root

    primaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
    primaryRadius: VLCStyle.dp(14, VLCStyle.scale)
    secondaryVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)
    secondaryRadius: VLCStyle.dp(3, VLCStyle.scale)
}
