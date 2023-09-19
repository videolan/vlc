/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Templates as T

import org.videolan.vlc 0.1

import "qrc:///style/"
import "."

T.Button {
    id: control

    property color color: "white"
    property string description

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    font.family: VLCIcons.fontFamily

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    //Accessible
    Accessible.onPressAction: control.clicked()
    Accessible.name: description

    // Tooltip
    T.ToolTip.visible: (hovered || visualFocus)
    T.ToolTip.delay: VLCStyle.delayToolTipAppear
    T.ToolTip.text: description

    contentItem: IconLabel {
        font: control.font
        color: control.color
        text: control.text
    }

    background: Item {
        implicitWidth: 10
        implicitHeight: 10
    }
}
