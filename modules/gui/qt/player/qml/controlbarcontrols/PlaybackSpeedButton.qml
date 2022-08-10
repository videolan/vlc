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
import QtQuick.Controls 2.4
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///player/" as P
import "qrc:///util/Helpers.js" as Helpers

Widgets.IconControlButton {
    id: root

    signal requestLockUnlockAutoHide(bool lock)

    readonly property bool _isCurrentViewPlayer: !paintOnly && (History.current.name === "player")

    text: I18n.qtr("Playback Speed")

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

    onClicked: popup.open()

    Popup {
        id: popup

        parent: root.paintOnly
                ? root // button is not part of main display (ToolbarEditorDialog)
                : root._isCurrentViewPlayer ? rootPlayer : g_root

        width: VLCStyle.dp(256, VLCStyle.scale)
        height: implicitHeight

        padding: VLCStyle.margin_small

        z: 1

        focus: true

        // Popup.CloseOnPressOutside doesn't work with non-model Popup on Qt < 5.15
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

        modal: true

        onOpened: {
            // update popup coordinates
            //
            // mapFromItem is affected by various properties of source and target objects which
            // can't be represented in a binding expression so a initial setting in object
            // definition (x: clamp(...)) doesn't work, so we set x and y on initial open
            x = Qt.binding(function () {
                // coords are mapped through root.parent so that binding is
                // generated based on root.x
                var position = parent.mapFromItem(root.parent, root.x, 0)

                var minimum = VLCStyle.margin_xxsmall + VLCStyle.applicationHorizontalMargin

                var maximum = parent.width - VLCStyle.applicationHorizontalMargin
                               - VLCStyle.margin_xxsmall - width

                return Helpers.clamp(position.x - ((width - root.width) / 2), minimum, maximum)
            })

            y = Qt.binding(function () {
                // coords are mapped through root.parent so that binding is
                // generated based on root.y
                var position = parent.mapFromItem(root.parent, 0, root.y)

                return position.y - popup.height - VLCStyle.margin_xxsmall
            })

            // player related --
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

        Overlay.modal: null

        background: Rectangle {
            color: colors.bg
            opacity: .85
        }

        contentItem: P.PlaybackSpeed {
            colors: root.colors

            Navigation.parentItem: root

            // NOTE: Mapping the right direction because the down action triggers the ComboBox.
            Navigation.rightItem: root
        }
    }

    T.Label {
        anchors.centerIn: parent
        font.pixelSize: VLCStyle.fontSize_normal

        text: !root.paintOnly ? I18n.qtr("%1x").arg(+Player.rate.toFixed(2))
                              : I18n.qtr("1x")

        // IconToolButton.background is a AnimatedBackground
        color: root.background.foregroundColor
    }
}
