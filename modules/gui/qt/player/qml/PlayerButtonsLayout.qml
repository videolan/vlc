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
import QtQuick.Layouts 1.3
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Widgets.NavigableFocusScope {
    id: playerButtonsLayout
    property alias model: buttonsRepeater.model
    property var defaultSize: VLCStyle.icon_medium
    property bool forceColors: false

    implicitWidth: buttonrow.implicitWidth
    implicitHeight: buttonrow.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: {
        if (!event.accepted)
            defaultKeyAction(event, 0)
    }

RowLayout{
    id: buttonrow
    property bool _focusGiven: false

    anchors.fill: parent

    Repeater{
        id: buttonsRepeater

        onItemRemoved: {
            if (item.focus) {
                buttonrow._focusGiven = false
            }
        }

        delegate: Loader{
            id: buttonloader

            sourceComponent: controlmodelbuttons.returnbuttondelegate(model.id)
            onLoaded: {
                if (!buttonrow._focusGiven) {
                    buttonloader.focus = true
                    buttonrow._focusGiven = true
                }
                buttonloader.item.focus = true

                if(buttonloader.item instanceof Widgets.IconToolButton)
                    buttonloader.item.size = model.size === PlayerControlBarModel.WIDGET_BIG ?
                                VLCStyle.icon_large : playerButtonsLayout.defaultSize

                //force buttons color
                if (playerButtonsLayout.forceColors) {
                    if ( buttonloader.item.color )
                        buttonloader.item.color = VLCStyle.colors.playerFg
                    if ( buttonloader.item.bgColor )
                        buttonloader.item.bgColor = VLCStyle.colors.setColorAlpha(VLCStyle.colors.playerBg, 0.8)
                    if ( buttonloader.item.borderColor )
                        buttonloader.item.borderColor = VLCStyle.colors.playerBorder
                }

                if (index > 0)
                    buttonloader.item.KeyNavigation.left = buttonrow.children[index-1].item
            }
        }
    }
}

}
