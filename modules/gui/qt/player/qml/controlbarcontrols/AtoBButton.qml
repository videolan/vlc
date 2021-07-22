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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Widgets.IconControlButton {
    id: abBtn

    size: VLCStyle.icon_medium
    checked: player.ABloopState !== PlayerController.ABLOOP_STATE_NONE
    onClicked: player.toggleABloopState()
    text: i18n.qtr("A to B")

    iconText: {
        switch(player.ABloopState) {
          case PlayerController.ABLOOP_STATE_A: return VLCIcons.atob_bg_b
          case PlayerController.ABLOOP_STATE_B: return VLCIcons.atob_bg_none
          case PlayerController.ABLOOP_STATE_NONE: return VLCIcons.atob_bg_ab
        }
    }

    Widgets.IconLabel {
        anchors.centerIn: abBtn.contentItem

        color: abBtn.colors.accent

        text: {
            switch(player.ABloopState) {
              case PlayerController.ABLOOP_STATE_A: return VLCIcons.atob_fg_a
              case PlayerController.ABLOOP_STATE_B: return VLCIcons.atob_fg_ab
              case PlayerController.ABLOOP_STATE_NONE: return ""
            }
        }
    }
}
