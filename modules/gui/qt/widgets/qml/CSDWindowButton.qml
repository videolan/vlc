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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11

import "qrc:///style/"


T.Button {
    id: control

    property color color: VLCStyle.colors.text
    property color hoverColor: VLCStyle.colors.windowCSDButtonBg
    property string iconTxt: ""
    property bool showHovered: false

    readonly property bool _paintHovered: control.hovered || showHovered

    padding: 0
    width: VLCStyle.dp(40, VLCStyle.scale)
    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight
    focusPolicy: Qt.NoFocus

    background: Rectangle {
        height: control.height
        width: control.width
        color: {
            if (control._paintHovered)
                return control.hoverColor

            if (control.pressed)
                return VLCStyle.colors.isThemeDark ? Qt.lighter(control.hoverColor, 1.2)
                                                   : Qt.darker(control.hoverColor, 1.2)

            return "transparent"
        }
    }

    contentItem: Item {
        IconLabel {
            id: icon
            anchors.centerIn: parent
            text: control.iconTxt
            font.pixelSize: VLCStyle.icon_CSD
            color: control.color
        }
    }
}
