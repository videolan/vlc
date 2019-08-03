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
import "qrc:///utils/" as Utils

RowLayout{
    id: buttonrow
    property bool _focusGiven: false
    property alias model: buttonsRepeater.model
    property var defaultSize: VLCStyle.icon_medium

    Repeater{
        id: buttonsRepeater
        delegate: Loader{
            id: buttonloader

            sourceComponent: controlmodelbuttons.returnbuttondelegate(model.id)
            onLoaded: {
                if (! buttonloader.item.acceptFocus)
                    return
                else
                    if (!buttonrow._focusGiven){
                        buttonloader.item.forceActiveFocus()
                        buttonrow._focusGiven = true
                    }
                if(buttonloader.item instanceof Utils.IconToolButton)
                    buttonloader.item.size = model.size === PlayerControlBarModel.WIDGET_BIG ?
                                VLCStyle.icon_large : defaultSize

                var buttonindex = DelegateModel.itemsIndex
                while(buttonindex > 0 && !(buttonrow.children[buttonindex-1].item.acceptFocus))
                    buttonindex = buttonindex-1

                if (buttonindex > 0)
                    buttonloader.item.KeyNavigation.left = buttonrow.children[buttonindex-1].item
            }
        }
    }
}
