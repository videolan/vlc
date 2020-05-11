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
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

FocusScope{
    id: widgetfscope
    x: teleWidget.x
    y: teleWidget.y
    width: teleWidget.width
    height: teleWidget.height

    property bool autohide: !paintOnly && !player.isTeletextAvailable
    property bool acceptFocus: autohide
    property bool paintOnly: false
    visible: !autohide

    property color color: VLCStyle.colors.text
    property color bgColor: VLCStyle.colors.bg

    RowLayout{
        id: teleWidget
        width: autohide ? 0 : VLCStyle.widthTeletext

        spacing: 0

        Widgets.IconToolButton{
            id: teleActivateBtn
            paintOnly: widgetfscope.paintOnly
            iconText: VLCIcons.tv
            text: i18n.qtr("Teletext activate")
            size: VLCStyle.icon_normal
            onClicked: player.teletextEnabled = !player.teletextEnabled
            color: widgetfscope.color
            checked: player.teletextEnabled
            focus: true
            KeyNavigation.right: player.teletextEnabled ?
                                     teleTransparencyBtn : blueKeyBtn.KeyNavigation.right
        }

        Widgets.IconToolButton{
            id: teleTransparencyBtn
            paintOnly: widgetfscope.paintOnly
            iconText: VLCIcons.tvtelx
            text: i18n.qtr("Teletext transparency")
            size: VLCStyle.icon_normal
            opacity: 0.5
            enabled: player.teletextEnabled
            onClicked: player.teletextTransparency = !player.teletextTransparency
            color: widgetfscope.color
            KeyNavigation.right: telePageNumber
        }

        Widgets.SpinBoxExt{
            id: telePageNumber
            enabled: player.teletextEnabled
            from: 100
            to: 899
            editable: true
            textColor: widgetfscope.color
            bgColor: widgetfscope.bgColor
            KeyNavigation.right: indexKeyBtn

            //only update the player teletext page when the user change the value manually
            property bool inhibitPageUpdate: true

            onValueChanged: {
                if (inhibitPageUpdate)
                    return
                player.teletextPage = value
            }

            Component.onCompleted: {
                value = player.teletextPage
                inhibitPageUpdate = false
            }

            Connections {
                target: player
                onTeletextPageChanged: {
                    telePageNumber.inhibitPageUpdate = true
                    telePageNumber.value = player.teletextPage
                    telePageNumber.inhibitPageUpdate = false
                }
            }
        }

        Widgets.IconToolButton{
            id: indexKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: i18n.qtr("Index key")
            onClicked: player.teletextPage = PlayerController.TELE_INDEX
            color: "grey"
            colorDisabled: "grey"
            KeyNavigation.right: redKeyBtn
        }
        Widgets.IconToolButton{
            id: redKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: i18n.qtr("Red key")
            onClicked: player.teletextPage = PlayerController.TELE_RED
            color: "red"
            colorDisabled: "grey"
            KeyNavigation.right: greenKeyBtn
        }
        Widgets.IconToolButton{
            id: greenKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: i18n.qtr("Green key")
            onClicked: player.teletextPage = PlayerController.TELE_GREEN
            color: "green"
            colorDisabled: "grey"
            KeyNavigation.right: yellowKeyBtn
        }
        Widgets.IconToolButton{
            id: yellowKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: i18n.qtr("Yellow key")
            onClicked: player.teletextPage = PlayerController.TELE_YELLOW
            color: "yellow"
            colorDisabled: "grey"
            KeyNavigation.right: blueKeyBtn
        }
        Widgets.IconToolButton{
            id: blueKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: i18n.qtr("Blue key")
            onClicked: player.teletextPage = PlayerController.TELE_BLUE
            color: "blue"
            colorDisabled: "grey"
        }
    }
}
