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
    id: mediaMenu

    Action { text: i18n.qtr("Open &File..." ) ;               onTriggered: dialogProvider.simpleOpenDialog();          icon.source:"qrc:/type/file-asym.svg"                        }
    Action { text: i18n.qtr( "&Open Multiple Files..." );     onTriggered: dialogProvider.openFileDialog();            icon.source:"qrc:/type/file-asym.svg";                       }
    Action { text: i18n.qtr( "Open D&irectory" );             onTriggered: dialogProvider.PLOpenDir();                 icon.source:"qrc:/type/folder-grey.svg";  shortcut: "Ctrl+F" }

    Action { text: i18n.qtr("Open &Disc...");                 onTriggered: dialogProvider.openDiscDialog();            icon.source:"qrc:/type/disc.svg";         shortcut: "Ctrl+D" }
    Action { text: i18n.qtr("Open &Network Stream...");       onTriggered: dialogProvider.openNetDialog();             icon.source:"qrc:/type/network.svg";      shortcut: "Ctrl+N" }
    Action { text: i18n.qtr("Open &Capture Device...");       onTriggered: dialogProvider.openCaptureDialog();         icon.source:"qrc:/type/capture-card.svg"; shortcut: "Ctrl+C" }
    Action { text: i18n.qtr("Open &Location from clipboard"); onTriggered: dialogProvider.openUrlDialog();                                                       shortcut: "Ctrl+V" }


    Widgets.MenuExt {
        id: recentsMenu
        title: i18n.qtr("Open &Recent Media")
        property bool hasData: true
        onAboutToShow:{
            recentsMenu.hasData = Boolean(recentsMedias.rowCount())
        }

        function moveItemToPos(item, pos)  {
            for ( var i = 0; i < recentsMenu.count; i++ ) {
                if (recentsMenu.itemAt(i) == item) {
                    recentsMenu.moveItem(i, pos)
                    return;
                }
            }
        }

        Repeater {
            model: recentsMedias

            Widgets.MenuItemExt {
                text: mrl
                onTriggered:{
                    mediaMenu.close() //needed since menuItem isn't a direct child of a menu
                    mainPlaylistController.append([mrl], true)
                }

                Shortcut {
                    sequence: "Ctrl+" + (index + 1)
                    onActivated:  mainPlaylistController.append([mrl], true)
                    context: Qt.ApplicationShortcut
                }
            }

            //replace last elements as the repeater won't keep the original
            //order of the menu when updated
            onItemAdded: {
                recentsMenu.moveItemToPos(clearAction, recentsMenu.count - 1)
                recentsMenu.moveItemToPos(clearSepId,  recentsMenu.count - 2)
            }

            onItemRemoved: {
                recentsMenu.moveItemToPos(clearAction, recentsMenu.count - 1)
                recentsMenu.moveItemToPos(clearSepId,  recentsMenu.count - 2)
            }
        }

        MenuSeparator {
            id: clearSepId
        }

        Widgets.MenuItemExt {
            id: clearAction
            text: i18n.qtr("Clear")
            enabled: recentsMenu.hasData
            onTriggered:recentsMedias.clear()
        }
    }

    Action { text: i18n.qtr("Save Playlist to &File...");     onTriggered: dialogProvider.savePlayingToPlaylist();     icon.source: "";                      shortcut: "Ctrl+Y" }
    Action { text: i18n.qtr("Conve&rt / Save..." );           onTriggered: dialogProvider.openAndTranscodingDialogs(); icon.source: "";                      shortcut: "Ctrl+R" }
    Action { text: i18n.qtr("&Stream..." );                   onTriggered: dialogProvider.openAndStreamingDialogs();   icon.source: "qrc:/menu/stream.svg";  shortcut: "Ctrl+S" }

    //Action { text: i18n.qtr( "&close to systray" );           onTriggered: dialogprovider.closeToSystray();                                                                     }
    Action { text: i18n.qtr( "Quit at the end of playlist" ); onTriggered: console.warn("FIXME");                                                            shortcut: "Ctrl+Q"; checkable: true; checked: true; }
    Action { text: i18n.qtr( "&Quit" );                       onTriggered: dialogProvider.quit();                      icon.source:"qrc:/menu/exit.svg";     shortcut: "Ctrl+Q" }
}
