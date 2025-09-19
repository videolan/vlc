/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

import VLC.Widgets as Widgets

// This item can be used as a layer effect.
// Make sure that the sampler name is set to "source" (default).
Widgets.DualKawaseBlur {
    id: root

    radius: 3

    postprocess: true
    tintStrength: Qt.colorEqual(tint, "transparent") ? 0.0 : 0.7
    noiseStrength: 0.02
    exclusionStrength: 0.09
}
