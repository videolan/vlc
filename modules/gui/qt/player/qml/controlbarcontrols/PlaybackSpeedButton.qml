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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///player/" as P
import "qrc:///util/Helpers.js" as Helpers

Widgets.IconControlButton {
    id: playbackSpeedButton

    readonly property bool _isCurrentViewPlayer: !paintOnly && (History.current.name === "player")

    size: VLCStyle.icon_medium
    text: I18n.qtr("Playback Speed")
    color: playbackSpeedPopup.visible ? colors.accent : colors.playerControlBarFg

    onClicked: playbackSpeedPopup.open()

    P.PlaybackSpeed {
        id: playbackSpeedPopup

        z: 1
        colors: playbackSpeedButton.colors
        focus: true
        parent: playbackSpeedButton.paintOnly
                ? playbackSpeedButton // button is not part of main display (ToolbarEditorDialog)
                : playbackSpeedButton._isCurrentViewPlayer ? rootPlayer : g_root

        Navigation.parentItem: playbackSpeedButton

        onOpened: {
            // update popup coordinates
            //
            // mapFromItem is affected by various properties of source and target objects
            // which can't be represented in a binding expression so a initial setting in
            // object definition (x: clamp(...)) doesn't work, so we set x and y on initial open
            x = Qt.binding(function () {
                // coords are mapped through playbackSpeedButton.parent so that binding is generated based on playbackSpeedButton.x
                var mappedParentCoordinates = parent.mapFromItem(playbackSpeedButton.parent, playbackSpeedButton.x, 0)
                return Helpers.clamp(mappedParentCoordinates.x  - ((width - playbackSpeedButton.width) / 2),
                                     VLCStyle.margin_xxsmall + VLCStyle.applicationHorizontalMargin,
                                     parent.width - VLCStyle.applicationHorizontalMargin - VLCStyle.margin_xxsmall - width)
            })

            y = Qt.binding(function () {
                // coords are mapped through playbackSpeedButton.parent so that binding is generated based on playbackSpeedButton.y
                var mappedParentCoordinates = parent.mapFromItem(playbackSpeedButton.parent, 0, playbackSpeedButton.y)
                return mappedParentCoordinates.y - playbackSpeedPopup.height - VLCStyle.margin_xxsmall
            })

            // player related --
            playerControlLayout.requestLockUnlockAutoHide(true, playerControlLayout)
            if (playbackSpeedButton._isCurrentViewPlayer)
                rootPlayer.menu = playbackSpeedPopup
        }

        onClosed: {
            playerControlLayout.requestLockUnlockAutoHide(false, playerControlLayout)
            playbackSpeedButton.forceActiveFocus()
            if (playbackSpeedButton._isCurrentViewPlayer)
                rootPlayer.menu = undefined
        }
    }

    T.Label {
        anchors.centerIn: parent
        font.pixelSize: VLCStyle.fontSize_normal
        text: !playbackSpeedButton.paintOnly ? I18n.qtr("%1x").arg(+Player.rate.toFixed(2)) : I18n.qtr("1x")
        color: playbackSpeedButton.background.foregroundColor // IconToolButton.background is a AnimatedBackground
    }
}
