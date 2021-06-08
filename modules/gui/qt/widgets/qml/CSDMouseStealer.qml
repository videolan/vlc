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

import QtQuick 2.15

Item {
    parent: g_root

    Repeater {
        model: [
            {
                x: 0,
                y: 0,
                width: g_root.width,
                height: mainInterface.csdBorderSize
            },
            {
                x: 0,
                y: 0,
                width: mainInterface.csdBorderSize,
                height: g_root.height
            },
            {
                x: g_root.width - mainInterface.csdBorderSize,
                y: 0,
                width: mainInterface.csdBorderSize,
                height: g_root.height
            },
            {
                x: 0,
                y: g_root.height - mainInterface.csdBorderSize,
                width: g_root.width,
                height: mainInterface.csdBorderSize
            }
        ]

        delegate: MouseArea {
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            hoverEnabled: true
        }
    }
}
