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

import "qrc:///style/"
import "qrc:///utils/" as Utils

Utils.MenuExt {
    Action {
        text: qsTr("&Always on Top")
        checkable: true
        checked: rootWindow.interfaceAlwaysOnTop
        onTriggered: rootWindow.interfaceAlwaysOnTop = !rootWindow.interfaceAlwaysOnTop
    }

    Action {
        text: qsTr("&Fullscreen Interface")
        checkable: true
        checked: rootWindow.interfaceFullScreen
        onTriggered: rootWindow.interfaceFullScreen = !rootWindow.interfaceFullScreen
    }

    Loader {
        active: medialib !== null
        sourceComponent:  Utils.MenuItemExt {
            text: qsTr("&View Items as Grid")
            checkable: true
            checked: medialib.gridView
            onTriggered: medialib.gridView = !medialib.gridView
        }
    }

    Utils.MenuExt {
        title: qsTr("Color Scheme")
        Repeater {
            model: VLCStyle.colors.colorSchemes
            Utils.MenuItemExt {
                text: modelData
                checkable: true
                checked: modelData === VLCStyle.colors.state
                onTriggered: VLCStyle.colors.state = modelData
            }
        }
    }

    MenuSeparator {}

    CheckableModelSubMenu{
        title: qsTr("Add Interface")
        model: rootWindow.extraInterfaces
    }

    /* FIXME unimplemented
    extensions
    */
}
