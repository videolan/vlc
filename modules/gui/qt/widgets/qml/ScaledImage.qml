/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

import QtQuick 2.11

import org.videolan.vlc 0.1
import org.videolan.compat 0.1

Image {
    id: root
    sourceSize: Qt.size(width * MainCtx.screen.devicePixelRatio
                    , height * MainCtx.screen.devicePixelRatio)

    property bool disableSmoothWhenIntegerUpscaling: false

    // TODO: Remove this Qt >= 5.14 (Binding.restoreMode == Binding.RestoreBindingOrValue)
    // Only required for the Binding to restore the value back
    readonly property bool _smooth: true
    smooth: _smooth

    BindingCompat on smooth {
        when: root.disableSmoothWhenIntegerUpscaling &&
              !((root.paintedWidth % root.implicitWidth) || (root.paintedHeight % root.implicitHeight))
        value: false
    }
}
