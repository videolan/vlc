/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 * Copyright (C) 2017 The Qt Company Ltd.
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

import VLC.Widgets as Widgets
import VLC.Style

T.RoundButton {
    id: control

    implicitWidth: text.length !== 1 ? Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                                implicitContentWidth + leftPadding + rightPadding)
                                     : implicitHeight // special case for single letter/icon to make it perfectly round. This should be safe due to `Text.HorizontalFit`
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    hoverEnabled: true

    padding: VLCStyle.dp(6, VLCStyle.scale)
    spacing: VLCStyle.dp(6, VLCStyle.scale)

    font.pixelSize: VLCStyle.icon_small

    //Accessible
    Accessible.onPressAction: control.clicked()

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton // ###

        enabled: control.enabled
        focused: control.visualFocus
        hovered: control.hovered
        pressed: control.down
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: theme.fg.primary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        fontSizeMode: (text.length !== 1) ? Text.FixedSize : Text.HorizontalFit
        Accessible.ignored: true
    }

    background: Widgets.AnimatedBackground {
        radius: control.radius
        enabled: control.enabled
        visible: !control.flat || control.down || control.checked || control.highlighted
        color: theme.bg.secondary // ###
        border.color: theme.border
    }
}
