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

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Widgets.IconControlButton {
    id: abBtn

    checked: Player.ABloopState !== Player.ABLOOP_STATE_NONE
    onClicked: Player.toggleABloopState()
    text: I18n.qtr("A to B")

    iconText: {
        switch(Player.ABloopState) {
          case Player.ABLOOP_STATE_A: return VLCIcons.atob_bg_b
          case Player.ABLOOP_STATE_B: return VLCIcons.atob_bg_none
          case Player.ABLOOP_STATE_NONE: return VLCIcons.atob_bg_ab
        }
    }

    Widgets.IconLabel {
        anchors.centerIn: abBtn.contentItem

        font.pixelSize: abBtn.size
        color: abBtn.colors.accent

        text: {
            switch(Player.ABloopState) {
              case Player.ABLOOP_STATE_A: return VLCIcons.atob_fg_a
              case Player.ABLOOP_STATE_B: return VLCIcons.atob_fg_ab
              case Player.ABLOOP_STATE_NONE: return ""
            }
        }
    }
}
