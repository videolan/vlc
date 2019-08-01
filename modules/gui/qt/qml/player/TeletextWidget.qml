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

import "qrc:///utils/" as Utils
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
        width: autohide ? 0 : VLCStyle.widthTeletext * scale

        spacing: 0

        Utils.IconToolButton{
            id: teleActivateBtn
            paintOnly: widgetfscope.paintOnly
            text: VLCIcons.tv
            size: VLCStyle.icon_normal
            onClicked: player.teletextEnabled = !player.teletextEnabled
            color: widgetfscope.color
            checked: player.teletextEnabled
            focus: true
            KeyNavigation.right: player.teletextEnabled ?
                                     teleTransparencyBtn : blueKeyBtn.KeyNavigation.right
        }

        Utils.IconToolButton{
            id: teleTransparencyBtn
            paintOnly: widgetfscope.paintOnly
            text: VLCIcons.tvtelx
            size: VLCStyle.icon_normal
            opacity: 0.5
            enabled: player.teletextEnabled
            onClicked: player.teletextTransparency = !player.teletextTransparency
            color: widgetfscope.color
            KeyNavigation.right: telePageNumber
        }

        Utils.SpinBoxExt{
            id: telePageNumber
            enabled: player.teletextEnabled
            from: 100
            to: 899
            editable: true
            textColor: widgetfscope.color
            bgColor: widgetfscope.bgColor
            onValueChanged: player.teletextPage = value
            KeyNavigation.right: indexKeyBtn
        }

        Utils.IconToolButton{
            id: indexKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_INDEX
            color: "grey"
            colorDisabled: "grey"
            KeyNavigation.right: redKeyBtn
        }
        Utils.IconToolButton{
            id: redKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_RED
            color: "red"
            colorDisabled: "grey"
            KeyNavigation.right: greenKeyBtn
        }
        Utils.IconToolButton{
            id: greenKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_GREEN
            color: "green"
            colorDisabled: "grey"
            KeyNavigation.right: yellowKeyBtn
        }
        Utils.IconToolButton{
            id: yellowKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_YELLOW
            color: "yellow"
            colorDisabled: "grey"
            KeyNavigation.right: blueKeyBtn
        }
        Utils.IconToolButton{
            id: blueKeyBtn
            paintOnly: widgetfscope.paintOnly
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_BLUE
            color: "blue"
            colorDisabled: "grey"
        }
    }
}
