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
import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface

FocusScope {
    id: root

    // Properties

    readonly property bool isViewMultiView: false

    property var sortModel: [
        { text: I18n.qtr("Title"),    criteria: "title"},
        { text: I18n.qtr("Album"),    criteria: "album_title" },
        { text: I18n.qtr("Artist"),   criteria: "main_artist" },
        { text: I18n.qtr("Duration"), criteria: "duration" },
        { text: I18n.qtr("Track"),    criteria: "track_number" },
        { text: I18n.qtr("Disc"),     criteria: "disc_number" }
    ]

    // Aliases

    property alias leftPadding: tracklistdisplay_id.leftPadding
    property alias rightPadding: tracklistdisplay_id.rightPadding

    property alias model: tracklistdisplay_id.model
    property alias selectionModel: tracklistdisplay_id.selectionDelegateModel

    function setCurrentItemFocus(reason) {
        tracklistdisplay_id.setCurrentItemFocus(reason);
    }

    MusicTrackListDisplay {
        id: tracklistdisplay_id

        anchors.fill: parent

        visible: model.count > 0
        focus: model.count > 0
        headerTopPadding: VLCStyle.margin_normal
        Navigation.parentItem: root
        Navigation.cancelAction: function() {
            if (tracklistdisplay_id.currentIndex <= 0)
                root.Navigation.defaultNavigationCancel()
            else
                tracklistdisplay_id.currentIndex = 0;
        }

        // To get blur effect while scrolling in mainview
        displayMarginEnd: g_mainDisplay.displayMargin

        backgroundColor: VLCStyle.colors.bg
    }

    EmptyLabelButton {
        anchors.fill: parent
        visible: !tracklistdisplay_id.model.hasContent
        focus: visible
        text: I18n.qtr("No tracks found\nPlease try adding sources, by going to the Browse tab")
        Navigation.parentItem: root
        cover: VLCStyle.noArtAlbumCover
    }
}
