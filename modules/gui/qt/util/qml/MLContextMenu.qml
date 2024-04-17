
/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import QtQml

import org.videolan.vlc 0.1

import "qrc:///util/Helpers.js" as Helpers

// @brief - a generic ML context menu
NativeMenu {
    id: root

    // Properties

    /* required */ property var model: null

    property string idDataRole: "id"

    property bool showPlayAsAudioAction: false

    // Signals

    signal showMediaInformation(int index)

    // Settings

    actions: [{
            "text": qsTr("Play"),
            "action": addAndPlay
        }, {
            "text": qsTr("Play as audio"),
            "action": playAsAudio,
            "visible": root.showPlayAsAudioAction
        }, {
            "text": qsTr("Enqueue"),
            "action": enqueue
        }, {
            "text": qsTr("Add to favorites"),
            "action": addFavorite,
            "visible": _showAddFavorite
        }, {
            "text": qsTr("Remove from favorites"),
            "action": removeFavorite,
            "visible": _showRemoveFavorite
        }, {
            "text": qsTr("Add to a playlist"),
            "action": addToAPlaylist
        }, {
            "text": qsTr("Mark as seen"),
            "action": markSeen,
            "visible": _showSeen
        }, {
            "text": qsTr("Mark as unseen"),
            "action": markUnseen,
            "visible": _showUnseen
        }, {
            "text": qsTr("Open Containing Folder"),
            "action": openContainingFolder,
            "visible": _openContainingFolder
        }, {
            "text": qsTr("Delete"),
            "action": deleteStream,
            "visible": _deleteStream
        }, {
            "text": qsTr("Information"),
            "action": _signalShowInformation,
            "visible": showInformationAvailable
        }, {
            "text": qsTr("Media Information"),
            "action": function(dataList, options, indexes) {
                DialogsProvider.mediaInfoDialog(dataList[0][idDataRole])
            },
            "visible": function(dataList, options, indexes) {
                return (dataList.length === 1)
                        && !(dataList[0][idDataRole].hasParent())
            }
        }
    ]

    // Events

    onRequestData: (requestID, indexes) => {
        model.getData(indexes, function (data) {
            setData(requestID, data)
        })
    }

    // Functions

    function addAndPlay(dataList, options, indexes) {
        model.ml.addAndPlay(_mlIDList(dataList), _playerOptions(options))
    }

    function playAsAudio(dataList, options, indexes) {
        model.ml.addAndPlay(_mlIDList(dataList), _playerOptions(options, ":no-video"))
    }

    function enqueue(dataList, options, indexes) {
        model.ml.addToPlaylist(_mlIDList(dataList), _playerOptions(options))
    }

    function addToAPlaylist(dataList, options, indexes) {
        DialogsProvider.playlistsDialog(_mlIDList(dataList))
    }

    function addFavorite(dataList, options, indexes) {
        model.setItemFavorite(indexes[0], true)
    }

    function removeFavorite(dataList, options, indexes) {
        model.setItemFavorite(indexes[0], false)
    }

    function markSeen(dataList, options, indexes) {
        model.setItemPlayed(indexes[0], true)
    }

    function markUnseen(dataList, options, indexes) {
        model.setItemPlayed(indexes[0], false)
    }

    function openContainingFolder(dataList, options, indexes) {
        const parentDir = model.getParentURL(indexes[0]);

        Qt.openUrlExternally(parentDir)
    }

    function deleteStream(dataList, options, indexes) {
        model.deleteStream(dataList[0][idDataRole])
    }

    function showInformationAvailable(dataList, options, indexes) {
        return indexes.length === 1
                && Helpers.isInteger(options?.["information"] ?? null)
    }

    // Private

    function _checkRole(dataList, role, expected) {
        if (dataList.length !== 1)
            return false

        if (!(role in dataList[0]))
            return false

        return (dataList[0][role] === expected)
    }

    function _showAddFavorite(dataList, options, indexes) {
        return _checkRole(dataList, "isFavorite", false)
    }

    function _showRemoveFavorite(dataList, options, indexes) {
        return _checkRole(dataList, "isFavorite", true)
    }

    function _showSeen(dataList, options, indexes) {
        return _checkRole(dataList, "isNew", true)
    }

    function _showUnseen(dataList, options, indexes) {
        return _checkRole(dataList, "isNew", false)
    }

    function _openContainingFolder(dataList, options, indexes) {
        return _checkRole(dataList, "isLocal", true)
    }

    function _deleteStream(dataList,options, indexes) {
        return _checkRole(dataList, "isDeletable", true)
    }

    function _signalShowInformation(dataList, options) {
        const index = options?.["information"] ?? null
        console.assert(Helpers.isInteger(index))
        showMediaInformation(index)
    }

    function _playerOptions(options, extraOptions) {
        const playerOpts = options?.["player-options"] ?? []
        return playerOpts.concat(extraOptions)
    }

    function _mlIDList(dataList) {
        const idList = []
        for (let i in dataList) {
            idList.push(dataList[i][idDataRole])
        }

        return idList
    }
}
