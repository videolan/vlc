/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

// This is useful especially for color animations, as we do not want the
// animations to run as soon as the theme is initialized, as the colors
// often adjusted depending on initialization, and to have debouncing.
Behavior {
    id: behavior

    // FIXME: Making this property `required` causes weird issues with Qt 6.2 when repeater is involved:
    property bool delayedEnabled: false // modifying `enabled` is not permitted, this property should be used instead

    Component.onCompleted: {
        behavior.enabled = false
    }

    Binding on enabled {
        delayed: true
        value: behavior.delayedEnabled
    }
}
