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

import QtQuick
import QtQuick.Controls

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers

Widgets.IconToolButton {
    id: control

    // Aliases

    property alias popup: popup

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    signal menuOpened(var menu)

    // Settings

    color: (popup.visible) ? control.colorContext.accent : control.colorContext.fg.primary

    // FIXME: We can't use upItem because a Popup is not an Item.
    Navigation.upAction: function() {
        if (popup.visible) {
            popup.forceActiveFocus(Qt.TabFocusReason)

            return
        }

        const parent = Navigation.parentItem;

        if (parent)
            parent.Navigation.defaultNavigationUp()
    }

    // Events

    onClicked: popup.open()

    // Children

    Popup {
        id: popup

        x: (parent.width - width) / 2
        y: -height - VLCStyle.margin_xxxsmall

        padding: VLCStyle.margin_small

        focus: true

        // This popup should not exceed the boundaries of the scene.
        // Setting margins to >=0 makes it sure that this is satisfied.
        margins: MainCtx.windowExtendedMargin

        modal: true

        // NOTE: Popup.CloseOnPressOutside doesn't work with non-model Popup on Qt < 5.15.
        closePolicy: (Popup.CloseOnPressOutside | Popup.CloseOnEscape)

        Overlay.modal: null

        // Events

        onOpened: {
            control.requestLockUnlockAutoHide(true)

            control.menuOpened(popup)
        }

        onClosed: {
            control.requestLockUnlockAutoHide(false)

            control.forceActiveFocus()

            control.menuOpened(null)
        }

        background: Rectangle {
            ColorContext {
                id: popupTheme
                palette: control.colorContext.palette
                colorSet: ColorContext.Window
            }

            radius: VLCStyle.dp(8, VLCStyle.scale)

            // NOTE: The opacity should be stronger on a light background for readability.
            color: (popupTheme.palette.isDark)
                   ? VLCStyle.setColorAlpha(popupTheme.bg.primary, 0.8)
                   : VLCStyle.setColorAlpha(popupTheme.bg.primary, 0.96)
        }
    }
}
