/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import "qrc:///style/"

Widgets.OverlayMenu {
    id: overlayMenu

    Action {
        id: playAction
        text: i18n.qtr("Play")
        onTriggered: mainPlaylistController.goTo(root.plmodel.getSelection()[0], true)
        property string fontIcon: VLCIcons.play
    }

    Action {
        id: streamAction
        text: i18n.qtr("Stream")
        onTriggered: dialogProvider.streamingDialog(root.plmodel.getSelection().map(function(i) { return root.plmodel.itemAt(i).url; }), false)
        property string fontIcon: VLCIcons.stream
    }

    Action {
        id: saveAction
        text: i18n.qtr("Save")
        onTriggered: dialogProvider.streamingDialog(root.plmodel.getSelection().map(function(i) { return root.plmodel.itemAt(i).url; }))
    }

    Action {
        id: infoAction
        text: i18n.qtr("Information")
        onTriggered: dialogProvider.mediaInfoDialog(root.plmodel.itemAt(root.plmodel.getSelection()[0]))
        icon.source: "qrc:/menu/info.svg"
    }

    Action {
        id: exploreAction
        text: i18n.qtr("Show Containing Directory")
        onTriggered: mainPlaylistController.explore(root.plmodel.itemAt(root.plmodel.getSelection()[0]))
        icon.source: "qrc:/type/folder-grey.svg"
    }

    Action {
        id: addFileAction
        text: i18n.qtr("Add File...")
        onTriggered: dialogProvider.simpleOpenDialog(false)
        icon.source: "qrc:/buttons/playlist/playlist_add.svg"
    }

    Action {
        id: addDirAction
        text: i18n.qtr("Add Directory...")
        onTriggered: dialogProvider.PLAppendDir()
        icon.source: "qrc:/buttons/playlist/playlist_add.svg"
    }

    Action {
        id: addAdvancedAction
        text: i18n.qtr("Advanced Open...")
        onTriggered: dialogProvider.PLAppendDialog()
        icon.source: "qrc:/buttons/playlist/playlist_add.svg"
    }

    Action {
        id: savePlAction
        text: i18n.qtr("Save Playlist to File...")
        onTriggered: dialogProvider.savePlayingToPlaylist();
    }

    Action {
        id: clearAllAction
        text: i18n.qtr("Clear Playlist")
        onTriggered: mainPlaylistController.clear()
        icon.source: "qrc:/toolbar/clear.svg"
    }

    Action {
        id: selectAllAction
        text: i18n.qtr("Select All")
        onTriggered: root.plmodel.selectAll()
    }

    Action {
        id: shuffleAction
        text: i18n.qtr("Shuffle Playlist")
        onTriggered: mainPlaylistController.shuffle()
        icon.source: "qrc:/buttons/playlist/shuffle_on.svg"
    }

    Action {
        id: sortAction
        text: i18n.qtr("Sort")
        property alias model: overlayMenu.sortMenu
    }

    Action {
        id: selectTracksAction
        text: i18n.qtr("Select Tracks")
        onTriggered: view.mode = "select"
    }

    Action {
        id: moveTracksAction
        text: i18n.qtr("Move Selection")
        onTriggered: view.mode = "move"
    }

    Action {
        id: deleteAction
        text: i18n.qtr("Remove Selected")
        onTriggered: view.onDelete()
    }

    property var rootMenu: ({

                                title: i18n.qtr("Playlist Menu"),
                                entries: [
                                    playAction,
                                    streamAction,
                                    saveAction,
                                    infoAction,
                                    exploreAction,
                                    addFileAction,
                                    addDirAction,
                                    addAdvancedAction,
                                    savePlAction,
                                    clearAllAction,
                                    selectAllAction,
                                    shuffleAction,
                                    sortAction,
                                    selectTracksAction,
                                    moveTracksAction,
                                    deleteAction
                                ]
                            })

    property var rootMenu_PLEmpty: ({
                                        title: i18n.qtr("Playlist Menu"),
                                        entries: [
                                            addFileAction,
                                            addDirAction,
                                            addAdvancedAction
                                        ]
                                    })

    property var rootMenu_noSelection: ({
                                            title: i18n.qtr("Playlist Menu"),
                                            entries: [
                                                addFileAction,
                                                addDirAction,
                                                addAdvancedAction,
                                                savePlAction,
                                                clearAllAction,
                                                sortAction,
                                                selectTracksAction
                                            ]
                                        })

    model: {
        if (root.plmodel.count === 0)
            rootMenu_PLEmpty
        else if (root.plmodel.selectedCount === 0)
            rootMenu_noSelection
        else
            rootMenu
    }

    // Sort menu:

    function sortOrderMarkRetriever(key) {
        if (key === mainPlaylistController.sortKey) {
            return (mainPlaylistController.sortOrder === PlaylistControllerModel.SORT_ORDER_ASC ? "↓" : "↑")
        }
        else {
            return null
        }
    }

    Action {
        id: sortTitleAction
        text: i18n.qtr("Title")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_TITLE
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortDurationAction
        text: i18n.qtr("Duration")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_DURATION
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortArtistAction
        text: i18n.qtr("Artist")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_ARTIST
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortAlbumAction
        text: i18n.qtr("Album")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_ALBUM
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortAlbumArtistAction
        text: i18n.qtr("Album Artist")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_ALBUM_ARTIST
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortGenreAction
        text: i18n.qtr("Genre")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_GENRE
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortDateAction
        text: i18n.qtr("Date")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_DATE
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortTrackNumberAction
        text: i18n.qtr("Track Number")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_TRACK_NUMBER
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortURLAction
        text: i18n.qtr("URL")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_URL
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    Action {
        id: sortRatingAction
        text: i18n.qtr("Rating")
        onTriggered: mainPlaylistController.sort(key)
        readonly property int key: PlaylistControllerModel.SORT_KEY_RATING
        readonly property string marking: sortOrderMarkRetriever(key)
        readonly property bool tickMark: (key === mainPlaylistController.sortKey)
    }

    property var sortMenu: ({
                                title: i18n.qtr("Sort Menu"),
                                entries: [
                                    sortTitleAction,
                                    sortDurationAction,
                                    sortArtistAction,
                                    sortAlbumAction,
                                    sortAlbumArtistAction,
                                    sortGenreAction,
                                    sortDateAction,
                                    sortTrackNumberAction,
                                    sortURLAction,
                                    sortRatingAction
                                ]
                            })


}
