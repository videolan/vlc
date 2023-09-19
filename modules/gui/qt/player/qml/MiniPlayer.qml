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
import QtQuick

import org.videolan.vlc 0.1

import "qrc:///style/"

ControlBar {
    id: root

    visible: animation.running || (state === "inViewport")

    anchors.bottomMargin: (state === "outViewport") ? -_delayedImplicitHeight : 0

    state: (Player.playingState === Player.PLAYING_STATE_STOPPED) ? "outViewport"
                                                                  : "inViewport"

    textPosition: (MainCtx.pinVideoControls) ? ControlBar.TimeTextPosition.LeftRightSlider
                                             : ControlBar.TimeTextPosition.Hide

    sliderHeight: (MainCtx.pinVideoControls) ? VLCStyle.heightBar_xxsmall
                                             : VLCStyle.dp(3, VLCStyle.scale)

    bookmarksHeight: (MainCtx.pinVideoControls) ? VLCStyle.controlBarBookmarksHeight
                                                : VLCStyle.icon_xsmall * 0.7

    identifier: PlayerControlbarModel.Miniplayer

    property real _delayedImplicitHeight

    onImplicitHeightChanged: {
        if (!animation.running) {
            // Animation should not be based on the implicit height change
            // but rather the visibility state:
            behavior.enabled = false
            Qt.callLater(() => { behavior.enabled = true })
        }
    }

    Binding on _delayedImplicitHeight {
        // eliminate intermediate adjustments until implicit height is calculated fully
        // we can not delay on component load because we do not want twitching
        // NOTE: The delay here can be removed, as long as a direct height is set
        //       for the whole control instead of implicit height.
        delayed: behavior.enabled
        value: root.implicitHeight
    }

    Behavior on anchors.bottomMargin {
        id: behavior
        enabled: false
        NumberAnimation {
            id: animation
            easing.type: Easing.InOutSine
            duration: VLCStyle.duration_long
        }
    }
}

