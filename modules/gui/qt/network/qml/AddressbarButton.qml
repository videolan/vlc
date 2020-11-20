
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
import QtQuick 2.11
import QtQuick.Controls 2.4

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

AbstractButton {
    id: button

    property bool onlyIcon: true
    property bool highlighted: false

    font.pixelSize: onlyIcon ? VLCIcons.pixelSize(VLCStyle.icon_normal) : VLCStyle.fontSize_large
    padding: VLCStyle.margin_xxsmall
    width: implicitWidth
    height: implicitHeight

    contentItem: contentLoader.item
    background: Rectangle {
        color: (button.hovered || button.activeFocus) ? VLCStyle.colors.accent : "transparent"
    }

    Loader {
        id: contentLoader

        sourceComponent: button.onlyIcon ? iconTextContent : textContent
    }

    Component {
        id: iconTextContent

        Widgets.IconLabel {
            text: button.text
            elide: Text.ElideRight
            font.pixelSize: button.font.pixelSize
            color: VLCStyle.colors.text
            opacity: (button.highlighted  || button.hovered || button.activeFocus) ? 1 : .6
            verticalAlignment: Text.AlignVCenter
        }
    }

    Component {
        id: textContent

        Label {
            text: button.text
            elide: Text.ElideRight
            font.pixelSize: button.font.pixelSize
            font.weight: button.highlighted ? Font.DemiBold : Font.Normal
            color: VLCStyle.colors.text
            opacity: (button.highlighted || button.hovered || button.activeFocus) ? 1 : .6
            verticalAlignment: Text.AlignVCenter
        }
    }
}
