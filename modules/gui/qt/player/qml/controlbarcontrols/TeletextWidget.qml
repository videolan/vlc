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
import QtQuick.Layouts 1.11
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

    property bool autohide: !paintOnly && !Player.isTeletextAvailable
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
            text: I18n.qtr("Teletext activate")
            size: VLCStyle.icon_normal
            onClicked: Player.teletextEnabled = !Player.teletextEnabled
            color: widgetfscope.color
            checked: Player.teletextEnabled
            focus: true

            Navigation.parentItem: widgetfscope
            Navigation.rightItem: Player.teletextEnabled ? teleTransparencyBtn : null
        }

        Widgets.IconToolButton{
            id: teleTransparencyBtn
            paintOnly: widgetfscope.paintOnly
            iconText: VLCIcons.tvtelx
            text: I18n.qtr("Teletext transparency")
            size: VLCStyle.icon_normal
            opacity: 0.5
            enabled: Player.teletextEnabled
            onClicked: Player.teletextTransparency = !Player.teletextTransparency
            color: widgetfscope.color

            Navigation.parentItem: widgetfscope
            Navigation.leftItem: teleActivateBtn
            Navigation.rightItem: telePageNumber
        }

        Widgets.SpinBoxExt{
            id: telePageNumber
            enabled: Player.teletextEnabled
            from: 100
            to: 899
            editable: true
            textColor: widgetfscope.color
            bgColor: widgetfscope.bgColor

            Navigation.parentItem: widgetfscope
            Navigation.leftItem: teleTransparencyBtn
            Navigation.rightItem: indexKeyBtn

            //only update the player teletext page when the user change the value manually
            property bool inhibitPageUpdate: true

            onValueChanged: {
                if (inhibitPageUpdate)
                    return
                Player.teletextPage = value
            }

            Component.onCompleted: {
                value = Player.teletextPage
                inhibitPageUpdate = false
            }

            Connections {
                target: Player
                onTeletextPageChanged: {
                    telePageNumber.inhibitPageUpdate = true
                    telePageNumber.value = Player.teletextPage
                    telePageNumber.inhibitPageUpdate = false
                }
            }
        }

        Widgets.IconToolButton{
            id: indexKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: Player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Index key")
            onClicked: Player.teletextPage = Player.TELE_INDEX
            color: "grey"
            colorDisabled: "grey"

            Navigation.parentItem: widgetfscope
            Navigation.leftItem: telePageNumber
            Navigation.rightItem: redKeyBtn
        }
        Widgets.IconToolButton{
            id: redKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: Player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Red key")
            onClicked: Player.teletextPage = Player.TELE_RED
            color: "red"
            colorDisabled: "grey"

            Navigation.parentItem: widgetfscope
            Navigation.leftItem: indexKeyBtn
            Navigation.rightItem: greenKeyBtn
        }
        Widgets.IconToolButton{
            id: greenKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: Player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Green key")
            onClicked: Player.teletextPage = Player.TELE_GREEN
            color: "green"
            colorDisabled: "grey"

            Navigation.parentItem: widgetfscope
            Navigation.leftItem: redKeyBtn
            Navigation.rightItem: yellowKeyBtn
        }
        Widgets.IconToolButton{
            id: yellowKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: Player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Yellow key")
            onClicked: Player.teletextPage = Player.TELE_YELLOW
            color: "yellow"
            colorDisabled: "grey"

            Navigation.parentItem: widgetfscope
            Navigation.leftItem: greenKeyBtn
            Navigation.rightItem: blueKeyBtn
        }
        Widgets.IconToolButton{
            id: blueKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: Player.teletextEnabled
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Blue key")
            onClicked: Player.teletextPage = Player.TELE_BLUE
            color: "blue"
            colorDisabled: "grey"

            Navigation.parentItem: widgetfscope
            Navigation.leftItem: yellowKeyBtn
        }
    }
}
