/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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
pragma Singleton
import QtQuick

import org.videolan.vlc 0.1

Item {
    id: root

    property real uiTransluency: (enabled && MainCtx.intfMainWindow.active) ? 1 : 0

    enabled: MainCtx.hasAcrylicSurface

    Behavior on uiTransluency {
        NumberAnimation {
            duration: VLCStyle.duration_long
            easing.type: Easing.InOutSine
        }
    }

    Binding {
        when: root.enabled
        target: MainCtx
        property: "acrylicActive"
        value: root.uiTransluency != 0
    }
}
