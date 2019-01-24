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

import "qrc:///utils/" as Utils

Utils.MenuExt {
    property bool isPlaying: player.playingState != PlayerController.PLAYING_STATE_STOPPED


    CheckableModelSubMenu {
        title: qsTr("Title")
        enabled: player.isPlaying
        model: player.titles
    }


    CheckableModelSubMenu {
        title: qsTr("Chapters");
        enabled: player.isPlaying
        model: player.chapters
    }

    CheckableModelSubMenu {
        title: qsTr("Program");
        enabled: player.isPlaying
        model: player.programs
    }

    /*
    //FIXME unimplemented
    Menu {
        title: qsTr("Custom Bookmarks");
        enabled: isPlaying
    }
    */

    MenuSeparator { }

    Utils.MenuExt {
        title: qsTr("Renderer");
    }

    MenuSeparator { }

    Utils.MenuExt {
        title: qsTr("Speed");
        enabled: isPlaying
        Action { text: qsTr("&Faster");         onTriggered: player.faster();       icon.source: "qrc:/toolbar/faster2.svg";    }
        Action { text: qsTr("&Faster (fine)");  onTriggered: player.littlefaster(); icon.source: "qrc:/toolbar/faster2.svg";    }
        Action { text: qsTr("N&ormal Speed");   onTriggered: player.normalRate();                                               }
        Action { text: qsTr("Slo&wer");         onTriggered: player.slower();       icon.source: "qrc:/toolbar/slower2.svg";    }
        Action { text: qsTr("Slo&wer (fine)");  onTriggered: player.littleslower(); icon.source: "qrc:/toolbar/slower2.svg";    }
    }

    MenuSeparator { }

    Action { text: qsTr("&Jump Forward");          enabled: player.isPlaying; onTriggered: player.jumpFwd();                icon.source: "qrc:/toolbar/skip_fw.svg";   }
    Action { text: qsTr("Jump Bac&kward");         enabled: player.isPlaying; onTriggered: player.jumpBwd();                icon.source: "qrc:/toolbar/skip_back.svg"; }
    Action { text: qsTr("Jump to Specific &Time"); enabled: player.isPlaying; onTriggered: dialogProvider.gotoTimeDialog();                                    }

    MenuSeparator { }

    Action { text: qsTr("Play");     enabled: player.isPlaying ; onTriggered: mainPlaylistController.play();     icon.source: "qrc:/toolbar/play_b.svg";     }
    Action { text: qsTr("Pause");    enabled: player.isPlaying ; onTriggered: mainPlaylistController.pause();    icon.source: "qrc:/toolbar/pause_b.svg";    }
    Action { text: qsTr("Stop");     enabled: player.isPlaying ; onTriggered: mainPlaylistController.stop();     icon.source: "qrc:/toolbar/stop_b.svg";     }
    Action { text: qsTr("Previous"); enabled: player.isPlaying ; onTriggered: mainPlaylistController.previous(); icon.source: "qrc:/toolbar/previous_b.svg"; }
    Action { text: qsTr("Next");     enabled: player.isPlaying ; onTriggered: mainPlaylistController.next();     icon.source: "qrc:/toolbar/next_b.svg";     }
    Action { text: qsTr("Record");   enabled: player.isPlaying ; onTriggered: player.record();         icon.source: "qrc:/toolbar/record.svg";     }
}
