/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
import QtQuick.Controls 2.4

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers

Widgets.IconControlButton {
    id: root

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    // Properties

    // Private

    readonly property bool _isCurrentViewPlayer: (paintOnly === false
                                                  &&
                                                  History.current.name === "player")

    // Settings

    enabled: Player.isTeletextAvailable

    iconText: VLCIcons.tv

    text: I18n.qtr("Teletext")

    color: (popup.visible) ? colors.accent : colors.playerControlBarFg

    // FIXME: We can't use upItem because a Popup is not an Item.
    Navigation.upAction: function() {
        if (popup.visible) {
            popup.forceActiveFocus(Qt.TabFocusReason)

            return
        }

        var parent = Navigation.parentItem;

        if (parent)
            parent.Navigation.defaultNavigationUp()
    }

    // Events

    onClicked: popup.open()

    // Connections

    Connections {
        target: (popup.visible) ? popup.parent : null

        onWidthChanged: _updatePosition()
        onHeightChanged: _updatePosition()
    }

    // Functions

    // Private

    function _updatePosition() {
        var parent = popup.parent

        var position = parent.mapFromItem(root, x, y)

        var popupX = Math.round(position.x - ((popup.width - width) / 2))

        var minimum = VLCStyle.applicationHorizontalMargin + VLCStyle.margin_xxsmall

        var maximum = parent.width - popup.width - minimum

        popup.x = Helpers.clamp(popupX, minimum, maximum)

        popup.y = position.y - popup.height - VLCStyle.margin_xxsmall
    }

    // Children

    Popup {
        id: popup

        parent: (root._isCurrentViewPlayer) ? rootPlayer : g_root

        width: VLCStyle.dp(256, VLCStyle.scale)
        height: implicitHeight

        padding: VLCStyle.margin_small

        z: 1

        focus: true

        modal: true

        // NOTE: Popup.CloseOnPressOutside doesn't work with non-model Popup on Qt < 5.15.
        closePolicy: (Popup.CloseOnPressOutside | Popup.CloseOnEscape)

        Overlay.modal: null

        onOpened: {
            root._updatePosition()

            root.requestLockUnlockAutoHide(true)

            if (root._isCurrentViewPlayer)
                rootPlayer.menu = popup
        }

        onClosed: {
            root.requestLockUnlockAutoHide(false)

            root.forceActiveFocus()

            if (root._isCurrentViewPlayer)
                rootPlayer.menu = undefined
        }

        onWidthChanged: if (visible) root._updatePosition()
        onHeightChanged: if (visible) root._updatePosition()

        background: Rectangle {
            opacity: 0.85

            color: colors.bg
        }

        contentItem: TeletextWidget {
            colors: root.colors

            Navigation.parentItem: root

            Navigation.downItem: root
        }
    }
}
