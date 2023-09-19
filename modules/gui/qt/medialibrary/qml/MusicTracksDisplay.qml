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
import QtQuick
import QtQuick.Controls
import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface

FocusScope {
    id: root

    // Properties

    readonly property bool hasGridListMode: false

    property var pagePrefix: []

    property var sortModel: [
        { text: qsTr("Title"),    criteria: "title"},
        { text: qsTr("Album"),    criteria: "album_title" },
        { text: qsTr("Artist"),   criteria: "main_artist" },
        { text: qsTr("Duration"), criteria: "duration" },
        { text: qsTr("Track"),    criteria: "track_number" },
        { text: qsTr("Disc"),     criteria: "disc_number" }
    ]

    // Aliases

    property alias leftPadding: tracklistdisplay_id.leftPadding
    property alias rightPadding: tracklistdisplay_id.rightPadding

    property alias isSearchable: tracklistdisplay_id.isSearchable
    property alias model: tracklistdisplay_id.model
    property alias selectionModel: tracklistdisplay_id.selectionModel

    function setCurrentItemFocus(reason) {
        tracklistdisplay_id.setCurrentItemFocus(reason);
    }

    MusicTrackListDisplay {
        id: tracklistdisplay_id

        anchors.fill: parent

        visible: model.count > 0
        focus: model.count > 0

        header: Widgets.ViewHeader {
            view: tracklistdisplay_id

            text: qsTr("Tracks")
        }

        searchPattern: MainCtx.search.pattern
        sortOrder: MainCtx.sort.order
        sortCriteria: MainCtx.sort.criteria

        Navigation.parentItem: root
        Navigation.cancelAction: function() {
            if (tracklistdisplay_id.currentIndex <= 0)
                root.Navigation.defaultNavigationCancel()
            else
                tracklistdisplay_id.currentIndex = 0;
        }

        // To get blur effect while scrolling in mainview
        displayMarginEnd: g_mainDisplay.displayMargin
    }

    Widgets.EmptyLabelButton {
        anchors.fill: parent
        visible: !tracklistdisplay_id.model.loading && (tracklistdisplay_id.model.count <= 0)
        focus: visible
        text: qsTr("No tracks found\nPlease try adding sources, by going to the Browse tab")
        Navigation.parentItem: root
        cover: VLCStyle.noArtAlbumCover
    }
}
