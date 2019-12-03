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

import "qrc:///widgets/" as Widgets

//main menus, to be used as a dropdown menu
Widgets.MenuExt {    
    id: mainDropdownMenu
    //make the menu modal, as we are not attached to a QQuickWindow
    modal: true
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape


    MediaMenu { title: i18n.qtr("&Media") }
    PlaybackMenu { title: i18n.qtr("&Playback") }
    AudioMenu { title: i18n.qtr("&Audio") }
    VideoMenu { title: i18n.qtr("&Video") }
    SubtitleMenu { title: i18n.qtr("&Subtitle") }
    ToolsMenu { title: i18n.qtr("&Tools") }
    ViewMenu { title: i18n.qtr("V&iew") }
    HelpMenu { title: i18n.qtr("&Help") }

    function openBelow(obj) {
        this.x = (obj.x + obj.width / 2) - this.width / 2
        this.y = obj.y + obj.height
        this.open()
    }

    function openAbove(obj) {
        this.x = (obj.x + obj.width / 2) - this.width / 2
        this.y = obj.y - this.height
        this.open()
    }
}
