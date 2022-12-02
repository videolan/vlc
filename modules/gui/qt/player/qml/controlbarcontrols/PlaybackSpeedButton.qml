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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///player/" as P

ControlButtonPopup {
    id: root

    popup.width: VLCStyle.dp(256, VLCStyle.scale)

    text: I18n.qtr("Playback Speed")

    popupContent: P.PlaybackSpeed {
        colorContext.palette: root.colorContext.palette

        Navigation.parentItem: root

        // NOTE: Mapping the right direction because the down action triggers the ComboBox.
        Navigation.rightItem: root
    }

    // Children

    T.Label {
        anchors.centerIn: parent

        font.pixelSize: VLCStyle.fontSize_normal

        text: !root.paintOnly ? I18n.qtr("%1x").arg(+Player.rate.toFixed(2))
                              : I18n.qtr("1x")

        // IconToolButton.background is a AnimatedBackground
        color: root.background.foregroundColor
    }
}
