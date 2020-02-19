/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Button {
    id: contextButton
    width: VLCStyle.icon_normal
    height: VLCStyle.icon_normal
    text: VLCIcons.ellipsis
    font.family: VLCIcons.fontFamily
    font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_medium)

    property alias color: contextButtonContent.color
    property alias backgroundColor: contextButtonBg.color

    hoverEnabled: true
    background: Rectangle {
        id: contextButtonBg
        anchors.fill: contextButton
        color: "transparent"
    }
    contentItem: Text {
        id: contextButtonContent
        text: contextButton.text
        font: contextButton.font
        color: VLCStyle.colors.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        //                                layer.enabled: true
        //                                layer.effect: DropShadow {
        //                                    color: VLCStyle.colors.text
        //                                }
    }
}
