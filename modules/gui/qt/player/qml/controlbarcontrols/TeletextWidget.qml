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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


T.Pane {
    id: root

    property VLCColors colors: VLCStyle.colors
    property bool paintOnly: false

    enabled: Player.isTeletextAvailable

    implicitWidth: Math.max(background ? background.implicitWidth : 0, contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(background ? background.implicitHeight : 0, contentHeight + topPadding + bottomPadding)

    contentWidth: teleWidget.implicitWidth
    contentHeight: teleWidget.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: Navigation.defaultKeyAction(event)

    Row {
        id: teleWidget
        anchors.fill: parent

        Widgets.IconToolButton{
            id: teleActivateBtn
            paintOnly: root.paintOnly
            iconText: VLCIcons.tv
            text: I18n.qtr("Teletext activate")
            size: VLCStyle.icon_normal
            onClicked: Player.teletextEnabled = !Player.teletextEnabled
            color: colors.text
            checked: Player.teletextEnabled
            focus: true
            toolTip.visible: hovered || visualFocus

            Navigation.parentItem: root
            Navigation.rightItem: teleTransparencyBtn
        }

        Widgets.IconToolButton{
            id: teleTransparencyBtn
            paintOnly: root.paintOnly
            iconText: VLCIcons.tvtelx
            text: I18n.qtr("Teletext transparency")
            size: VLCStyle.icon_normal
            opacity: 0.5
            onClicked: Player.teletextTransparency = !Player.teletextTransparency
            color: colors.text
            toolTip.visible: hovered || visualFocus

            Navigation.parentItem: root
            Navigation.leftItem: teleActivateBtn
            Navigation.rightItem: telePageNumber
        }

        Widgets.SpinBoxExt{
            id: telePageNumber
            from: 100
            to: 899
            editable: true
            textColor: colors.text
            bgColor: colors.bg

            Navigation.parentItem: root
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
            paintOnly: root.paintOnly
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Index key")
            onClicked: Player.teletextPage = Player.TELE_INDEX
            color: "grey"
            colorDisabled: "grey"
            toolTip.visible: hovered || visualFocus

            Navigation.parentItem: root
            Navigation.leftItem: telePageNumber
            Navigation.rightItem: redKeyBtn
        }
        Widgets.IconToolButton{
            id: redKeyBtn
            paintOnly: root.paintOnly
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Red key")
            onClicked: Player.teletextPage = Player.TELE_RED
            color: "red"
            colorDisabled: "grey"
            toolTip.visible: hovered || visualFocus

            Navigation.parentItem: root
            Navigation.leftItem: indexKeyBtn
            Navigation.rightItem: greenKeyBtn
        }
        Widgets.IconToolButton{
            id: greenKeyBtn
            paintOnly: root.paintOnly
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Green key")
            onClicked: Player.teletextPage = Player.TELE_GREEN
            color: "green"
            colorDisabled: "grey"
            toolTip.visible: hovered || visualFocus

            Navigation.parentItem: root
            Navigation.leftItem: redKeyBtn
            Navigation.rightItem: yellowKeyBtn
        }
        Widgets.IconToolButton{
            id: yellowKeyBtn
            paintOnly: root.paintOnly
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Yellow key")
            onClicked: Player.teletextPage = Player.TELE_YELLOW
            color: "yellow"
            colorDisabled: "grey"
            toolTip.visible: hovered || visualFocus

            Navigation.parentItem: root
            Navigation.leftItem: greenKeyBtn
            Navigation.rightItem: blueKeyBtn
        }
        Widgets.IconToolButton{
            id: blueKeyBtn
            paintOnly: root.paintOnly
            size: VLCStyle.icon_normal
            iconText: VLCIcons.record
            text: I18n.qtr("Blue key")
            onClicked: Player.teletextPage = Player.TELE_BLUE
            color: "blue"
            colorDisabled: "grey"
            toolTip.visible: hovered || visualFocus

            Navigation.parentItem: root
            Navigation.leftItem: yellowKeyBtn
        }
    }
}
