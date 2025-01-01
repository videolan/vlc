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
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Style

T.ToolTip {
    id: control

    x: 0
    y: -implicitHeight - VLCStyle.margin_xxxsmall

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: VLCStyle.margin_xsmall
    padding: VLCStyle.margin_xsmall

    font.pixelSize: VLCStyle.fontSize_normal

    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    Component.onCompleted: {
        if (MainCtx.useTopLevelWindowForToolTip()) {
            console.assert(typeof control.popupType === "number")
            control.popupType = 1 // Popup.Window
        }
    }

    // NOTE: The tool tip often moves around, particularly noticable
    //       with the attached (global) tool tip, and pointing tool
    //       tip. Ideally, we could override property `visible` and
    //       call `QQuickPopup::open()` with `Qt.callLater()`, but
    //       the property is marked as FINAL. At the same time,
    //       Qt disables the timer when the delay is 0, ideally we
    //       need a timer with 0 delay, but 1 millisecond should
    //       not be a big deal.
    // NOTE: Mispositioned tool tip upon being visible is observed
    //       especially with the independent window (`Popup.Window`)
    //       setting. We need to make sure that its position is
    //       adjusted before the tool tip is shown. For that reason,
    //       be careful to not use complex bindings for the position
    //       and text, as they may not be resolved as soon as the
    //       tool tip is shown (single event loop cycle).
    delay: 1

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Tooltip
    }

    contentItem: Text {
        text: control.text
        font: control.font

        color: theme.fg.primary
    }

    background: Rectangle {
        border.color: theme.border
        color: theme.bg.primary
    }
}
