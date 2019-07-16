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

    property bool autohide: !player.isTeletextAvailable
    property bool acceptFocus: autohide
    visible: !autohide

    RowLayout{
        id: teleWidget
        width: autohide ? 0 : VLCStyle.widthTeletext * scale

        spacing: 0

        Utils.IconToolButton{
            id: teleActivateBtn
            text: VLCIcons.tv
            size: VLCStyle.icon_normal
            onClicked: player.teletextEnabled = !player.teletextEnabled
            checked: player.teletextEnabled
            focus: true
            KeyNavigation.right: player.teletextEnabled ?
                                     teleTransparencyBtn : blueKeyBtn.KeyNavigation.right
        }

        Utils.IconToolButton{
            id: teleTransparencyBtn
            text: VLCIcons.tvtelx
            size: VLCStyle.icon_normal
            opacity: 0.5
            enabled: player.teletextEnabled
            onClicked: player.teletextTransparency = !player.teletextTransparency
            KeyNavigation.right: telePageNumber
        }

        Utils.SpinBoxExt{
            id: telePageNumber
            enabled: player.teletextEnabled
            from: 100
            to: 899
            editable: true
            onValueChanged: player.teletextPage = value
            KeyNavigation.right: indexKeyBtn
        }

        Utils.IconToolButton{
            id: indexKeyBtn
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_INDEX
            color: "grey"
            KeyNavigation.right: redKeyBtn
        }
        Utils.IconToolButton{
            id: redKeyBtn
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_RED
            color: enabled ? "red" : "grey"
            KeyNavigation.right: greenKeyBtn
        }
        Utils.IconToolButton{
            id: greenKeyBtn
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_GREEN
            color: enabled ? "green" : "grey"
            KeyNavigation.right: yellowKeyBtn
        }
        Utils.IconToolButton{
            id: yellowKeyBtn
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_YELLOW
            color: enabled ? "yellow" : "grey"
            KeyNavigation.right: blueKeyBtn
        }
        Utils.IconToolButton{
            id: blueKeyBtn
            enabled: player.teletextEnabled
            size: VLCStyle.icon_normal
            text: VLCIcons.record
            onClicked: player.teletextPage = PlayerController.TELE_BLUE
            color: enabled ? "blue" : "grey"
        }
    }
}
