/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
import QtQuick.Templates as T

import VLC.Style

T.ScrollBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: VLCStyle.dp(2, VLCStyle.scale)
    visible: policy !== T.ScrollBar.AlwaysOff && ((background && background.opacity > 0.0) || (contentItem && contentItem.opacity > 0.0))
    minimumSize: horizontal ? (height / width) : (width / height)

    // We don't want to show anything if scrolling is not possible (content size less than
    // or equal to flickable size), unless `ScrollBar.AlwaysOn` is used (not by default):
    readonly property bool _shown: (policy === T.ScrollBar.AlwaysOn) || (control.size < 1.0)

    // active is not used here, because it is set when the attached Flickable is moving.
    // interacting is only set when the scroll bar itself is interacted.
    readonly property bool interacting: (interactive && (control.hovered || control.pressed))

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        // focused: control.activeFocus // irrelevant
        enabled: control.enabled

        // Do not change the colors when pressed or hovered, similar to WinUI3:
        pressed: false
        hovered: true
    }

    background: Rectangle {
        color: theme.bg.primary
        radius: (control.horizontal ? height : width) / 2

        visible: (control._shown && control.interacting)
    }

    contentItem: Rectangle {
        implicitWidth: control.interacting ? VLCStyle.scrollBarInteractingSize : VLCStyle.scrollBarNonInteractingSize
        implicitHeight: control.interacting ? VLCStyle.scrollBarInteractingSize : VLCStyle.scrollBarNonInteractingSize

        radius: (control.horizontal ? height : width) / 2
        color: theme.fg.secondary

        visible: control._shown

        component SizeBehavior : Behavior {
            NumberAnimation {
                easing.type: Easing.OutSine
                duration: VLCStyle.duration_veryShort
            }
        }

        SizeBehavior on implicitWidth {
            enabled: control.vertical
        }

        SizeBehavior on implicitHeight {
            enabled: control.horizontal
        }
    }
}
