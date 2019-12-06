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

import "qrc:///utils/" as Utils

Utils.MenuExt {
    CheckableModelSubMenu {
        title: qsTr("Video Track");
        enabled: player.isPlaying
        model: player.videoTracks
    }

    MenuSeparator { }

    Action {
        text: qsTr("Fullscreen")
        enabled: player.isPlaying
        checkable: true
        checked: player.fullscreen
        onTriggered: player.fullscreen = !player.fullscreen
    }

    Action {
        text: qsTr("Always Fit Window")
        enabled: player.isPlaying
        checkable: true
        checked: player.autoscale
        onTriggered: player.autoscale = !player.autoscale
    }

    Action {
        text: qsTr("Set as Wallpaper")
        enabled: player.isPlaying
        checkable: true
        checked: player.wallpaperMode
        onTriggered: player.wallpaperMode = !player.wallpaperMode;
    }

    MenuSeparator { }

    CheckableModelSubMenu {
        title: qsTr("Zoom")
        enabled: player.isPlaying
        model: player.zoom
    }

    CheckableModelSubMenu {
        title: qsTr("Aspect Ratio")
        enabled: player.isPlaying
        model: player.aspectRatio
    }

    CheckableModelSubMenu {
        title: qsTr("Crop")
        enabled: player.isPlaying
        model: player.crop
    }

    MenuSeparator { }

    CheckableModelSubMenu {
        title: qsTr("Deinterlace")
        enabled: player.isPlaying
        model: player.deinterlace
    }

    CheckableModelSubMenu {
        title: qsTr("Deinterlace Mode");
        enabled: player.isPlaying
        model: player.deinterlaceMode
    }

    MenuSeparator { }

    Action {
        text: qsTr("Take snapshot");
        enabled: player.isPlaying
        onTriggered: player.snapshot();
    }
}
