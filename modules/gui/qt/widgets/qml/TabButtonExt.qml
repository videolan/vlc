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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.3

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.TabButton {
    id: control
    text: model.displayText

    padding: 0

    width: implicitWidth
    height: implicitHeight
    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight

    property string iconTxt: ""
    property bool selected: false

    font.pixelSize: VLCStyle.fontSize_normal

    background: FocusBackground {
        height: control.height
        width: control.width
        active: (control.activeFocus || control.hovered)
    }

    contentItem: Item {
        implicitWidth: tabRow.implicitWidth
        implicitHeight: tabRow.implicitHeight

        Row {
            id: tabRow

            anchors.fill:  parent

            padding: VLCStyle.margin_xsmall
            spacing: VLCStyle.margin_xsmall

            Item {
                width: implicitWidth
                height: implicitHeight
                implicitWidth: VLCStyle.fontHeight_normal
                implicitHeight: VLCStyle.fontHeight_normal
                visible: control.iconTxt !== ""

                Widgets.IconLabel {
                    id: icon

                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    text: control.iconTxt
                    color: VLCStyle.colors.buttonText

                    font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_topbar)
                }
            }


            Widgets.MenuCaption {
                text: control.text
                color: VLCStyle.colors.text
            }
        }

        Rectangle {
            anchors {
                left: tabRow.left
                right: tabRow.right
                bottom: tabRow.bottom
            }
            height: 2
            visible: control.selected
            color: "transparent"
            border.color: VLCStyle.colors.accent
        }
    }
}
