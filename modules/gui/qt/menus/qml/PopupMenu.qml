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

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets

//main menus, to be used as a dropdown menu
Widgets.MenuExt {
    //make the menu modal, as we are not attached to a QQuickWindow
    modal: true
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

    Action { text: i18n.qtr("Play");     enabled: player.isPlaying ; onTriggered: mainPlaylistController.play();     icon.source: "qrc:/toolbar/play_b.svg";     }
    Action { text: i18n.qtr("Pause");    enabled: player.isPlaying ; onTriggered: mainPlaylistController.pause();    icon.source: "qrc:/toolbar/pause_b.svg";    }
    Action { text: i18n.qtr("Stop");     enabled: player.isPlaying ; onTriggered: mainPlaylistController.stop();     icon.source: "qrc:/toolbar/stop_b.svg";     }
    Action { text: i18n.qtr("Previous"); enabled: player.isPlaying ; onTriggered: mainPlaylistController.previous(); icon.source: "qrc:/toolbar/previous_b.svg"; }
    Action { text: i18n.qtr("Next");     enabled: player.isPlaying ; onTriggered: mainPlaylistController.next();     icon.source: "qrc:/toolbar/next_b.svg";     }
    Action { text: i18n.qtr("Record");   enabled: player.isPlaying ; onTriggered: player.toggleRecord();         icon.source: "qrc:/toolbar/record.svg";     }

    MenuSeparator {}
    Action { text: i18n.qtr("&Fullscreen Interface"); checkable: true; checked: mainInterface.interfaceFullScreen;  onTriggered: mainInterface.interfaceFullScreen = !mainInterface.interfaceFullScreen }

    MenuSeparator {}
    AudioMenu { title: i18n.qtr("&Audio") }
    VideoMenu { title: i18n.qtr("&Video") }
    SubtitleMenu { title: i18n.qtr("&Subtitle") }
    PlaybackMenu { title: i18n.qtr("&Playback") }

    MenuSeparator {}

    ViewMenu { title: i18n.qtr("V&iew") }
    ToolsMenu { title: i18n.qtr("&Tools") }
    MediaMenu { title: i18n.qtr("&Media") }

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
