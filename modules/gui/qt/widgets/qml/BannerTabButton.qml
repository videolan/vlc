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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.TabButton {
    id: control

    property color color: VLCStyle.colors.banner
    property color colorSelected: VLCStyle.colors.bg
    property bool showText: true

    text: model.displayText
    padding: 0
    width: control.showText ? VLCStyle.bannerTabButton_width_large : VLCStyle.banner_icon_size
    height: implicitHeight
    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight

    property string iconTxt: ""
    property bool selected: false

    background: Rectangle {
        height: control.height
        width: control.width
        color: (control.activeFocus || control.hovered) ? VLCStyle.colors.accent
                                                        :  (control.selected ? control.colorSelected : control.color)
        Behavior on color {
            ColorAnimation {
                duration: 128
            }
        }
    }

    contentItem: Item {
        implicitWidth: tabRow.implicitWidth
        implicitHeight: tabRow.implicitHeight

        RowLayout {
            id: tabRow

            anchors.centerIn: parent
            spacing: VLCStyle.margin_xsmall

            Widgets.IconLabel {
                id: icon

                text: control.iconTxt
                font.pixelSize: VLCIcons.pixelSize(VLCStyle.banner_icon_size)
                color: (control.activeFocus || control.hovered) ? VLCStyle.colors.accentText
                                                                : ((control.selected) ? VLCStyle.colors.accent : VLCStyle.colors.text)
            }

            Widgets.MenuLabel {
                id: txt
                visible: showText
                font.weight: (control.activeFocus || control.hovered || control.selected) ? Font.DemiBold : Font.Normal
                color: (control.activeFocus || control.hovered) ? VLCStyle.colors.accentText
                                                                : ((control.selected) ? VLCStyle.colors.text : VLCStyle.colors.menuCaption)
                text: control.text
            }
        }
    }
}
