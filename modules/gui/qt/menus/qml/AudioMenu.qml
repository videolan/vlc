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
    CheckableModelSubMenu {
        title: i18n.qtr("Audio &Track");
        enabled: player.isPlaying
        model: player.audioTracks
    }

    CheckableModelSubMenu {
        title: i18n.qtr("Audio &Device")
        model: player.audioDevices
    }

    CheckableModelSubMenu {
        title: i18n.qtr("&Stereo Mode");
        enabled: player.isPlaying
        model: player.audioStereoMode
    }

    MenuSeparator { }

    CheckableModelSubMenu {
        title: i18n.qtr("&Visualizations");
        enabled: player.isPlaying
        model: player.audioVisualization
    }

    MenuSeparator { }

    Action { text: i18n.qtr("Increase Volume"); enabled: player.isPlaying; onTriggered: player.setVolumeUp();   icon.source: "qrc:/toolbar/volume-high.svg";  }
    Action { text: i18n.qtr("Decrease Volume"); enabled: player.isPlaying; onTriggered: player.setVolumeDown(); icon.source: "qrc:/toolbar/volume-low.svg";   }
    Action { text: i18n.qtr("Mute");            enabled: player.isPlaying; onTriggered: player.toggleMuted();   icon.source: "qrc:/toolbar/volume-muted.svg"; }
}
