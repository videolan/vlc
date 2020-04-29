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
import "qrc:///widgets/" as Widgets

Widgets.MenuExt {
    id: viewMenu
    Action {
        text: i18n.qtr("Play&list")
        onTriggered: mainInterface.playlistVisible = !mainInterface.playlistVisible
    }
    Action {
        text: i18n.qtr("Docked Playlist")
        checkable: true
        checked: mainInterface.playlistDocked
        onTriggered: mainInterface.playlistDocked = !mainInterface.playlistDocked
    }
    Action {
        text: i18n.qtr("&Always on Top")
        checkable: true
        checked: mainInterface.interfaceAlwaysOnTop
        onTriggered: mainInterface.interfaceAlwaysOnTop = !mainInterface.interfaceAlwaysOnTop
    }

    Action {
        text: i18n.qtr("&Fullscreen Interface")
        checkable: true
        checked: mainInterface.interfaceFullScreen
        onTriggered: mainInterface.interfaceFullScreen = !mainInterface.interfaceFullScreen
    }

    Loader {
        active: medialib !== null
        sourceComponent:  Widgets.MenuItemExt {
            text: i18n.qtr("&View Items as Grid")
            checkable: true
            checked: medialib.gridView
            onTriggered: medialib.gridView = !medialib.gridView
        }
    }

    Widgets.MenuExt {
        title: i18n.qtr("Color Scheme")
        Repeater {
            model: VLCStyle.colors.colorSchemes
            Widgets.MenuItemExt {
                text: modelData
                checkable: true
                checked: modelData === VLCStyle.colors.state
                onTriggered: settings.VLCStyle_colors_state = modelData
            }
        }
    }

    MenuSeparator {}

    CheckableModelSubMenu{
        title: i18n.qtr("Add Interface")
        model: mainInterface.extraInterfaces
    }

    /* FIXME unimplemented
    extensions
    */
}
