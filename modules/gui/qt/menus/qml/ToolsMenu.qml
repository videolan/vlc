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

Widgets.MenuExt {
    Action { text: i18n.qtr("&Effects and Filters");    onTriggered: dialogProvider.extendedDialog();   icon.source: "qrc:/menu/settings.svg";    shortcut: "Ctrl+E" }
    Action { text: i18n.qtr("&Track Synchronization");  onTriggered: dialogProvider.synchroDialog();    icon.source: "qrc:/menu/settings.svg";                       }
    Action { text: i18n.qtr("Media &Information") ;     onTriggered: dialogProvider.mediaInfoDialog();  icon.source: "qrc:/menu/info.svg";        shortcut: "Ctrl+I" }
    Action { text: i18n.qtr("&Codec Information") ;     onTriggered: dialogProvider.mediaCodecDialog(); icon.source: "qrc:/menu/info.svg";        shortcut: "Ctrl+J" }
    Action { text: i18n.qtr("Program Guide");           onTriggered: dialogProvider.epgDialog();                                                                     }
    Action { text: i18n.qtr("&Messages");               onTriggered: dialogProvider.messagesDialog();   icon.source: "qrc:/menu/messages.svg";    shortcut: "Ctrl+M" }
    Action { text: i18n.qtr("Plu&gins and extensions"); onTriggered: dialogProvider.pluginDialog();                                                                  }
    MenuSeparator {}
    Action { text: i18n.qtr("Customise Interface");     onTriggered: dialogProvider.toolbarDialog();    icon.source: "qrc:/menu/preferences.svg";}
    Action { text: i18n.qtr("&Preferences");            onTriggered: dialogProvider.prefsDialog();      icon.source: "qrc:/menu/preferences.svg"; shortcut: "Ctrl+P" }
}
