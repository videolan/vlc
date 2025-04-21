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
import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Style
import VLC.Widgets as Widgets

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

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    // Aliases

    property alias leftPadding: tracklistdisplay_id.leftPadding
    property alias rightPadding: tracklistdisplay_id.rightPadding

    property alias isSearchable: tracklistdisplay_id.isSearchable
    property alias model: tracklistdisplay_id.model
    property alias selectionModel: tracklistdisplay_id.selectionModel

    property alias displayMarginBeginning: tracklistdisplay_id.displayMarginBeginning
    property alias displayMarginEnd: tracklistdisplay_id.displayMarginEnd

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

            visible: view.count > 0

            text: qsTr("Tracks")
        }

        searchPattern: MainCtx.search.pattern
        sortOrder: MainCtx.sort.order
        sortCriteria: MainCtx.sort.criteria

        fadingEdge.enableBeginningFade: root.enableBeginningFade
        fadingEdge.enableEndFade: root.enableEndFade

        Navigation.parentItem: root
        Navigation.cancelAction: function() {
            if (tracklistdisplay_id.currentIndex <= 0)
                root.Navigation.defaultNavigationCancel()
            else
                tracklistdisplay_id.currentIndex = 0;
        }
    }

    Widgets.EmptyLabelButton {
        anchors.centerIn: parent
        visible: !tracklistdisplay_id.model.loading && (tracklistdisplay_id.model.count <= 0)
        focus: visible
        text: qsTr("No tracks found\nPlease try adding sources")
        Navigation.parentItem: root
        cover: VLCStyle.noArtAlbumCover
    }
}
