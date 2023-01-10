
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
import QtQml 2.11

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
            "text": I18n.qtr("Play"),
            "action": addAndPlay
        }, {
            "text": I18n.qtr("Play as audio"),
            "action": playAsAudio,
            "visible": root.showPlayAsAudioAction
        }, {
            "text": I18n.qtr("Enqueue"),
            "action": enqueue
        }, {
            "text": I18n.qtr("Add to favorites"),
            "action": addFavorite,
            "visible": _showAddFavorite
        }, {
            "text": I18n.qtr("Remove from favorites"),
            "action": removeFavorite,
            "visible": _showRemoveFavorite
        }, {
            "text": I18n.qtr("Add to a playlist"),
            "action": addToAPlaylist
        }, {
            "text": I18n.qtr("Mark as seen"),
            "action": markSeen,
            "visible": _showSeen
        }, {
            "text": I18n.qtr("Mark as unseen"),
            "action": markUnseen,
            "visible": _showUnseen
        }, {
            "text": I18n.qtr("Information"),
            "action": _signalShowInformation,
            "visible": showInformationAvailable
        }]

    // Events

    onRequestData: {
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

    function showInformationAvailable(options, indexes) {
        return indexes.length === 1
                && Helpers.isInteger(Helpers.get(options, "information", null))
    }

    // Private

    function _showAddFavorite(options, indexes) {
        if (indexes.length !== 1)
            return false

        var isFavorite = model.getDataAt(indexes[0]).isFavorite

        // NOTE: Strictly comparing 'isFavorite' given it might be undefined.
        return (isFavorite === false)
    }

    function _showRemoveFavorite(options, indexes) {
        if (indexes.length !== 1)
            return false

        var isFavorite = model.getDataAt(indexes[0]).isFavorite

        // NOTE: Strictly comparing 'isFavorite' given it might be undefined.
        return (isFavorite === true)
    }

    function _showSeen(options, indexes) {
        if (indexes.length !== 1)
            return false

        var isNew = model.getDataAt(indexes[0]).isNew

        // NOTE: Strictly comparing 'isNew' given it might be undefined.
        return (isNew === true)
    }

    function _showUnseen(options, indexes) {
        if (indexes.length !== 1)
            return false

        var isNew = model.getDataAt(indexes[0]).isNew

        // NOTE: Strictly comparing 'isNew' given it might be undefined.
        return (isNew === false)
    }

    function _signalShowInformation(dataList, options) {
        var index = Helpers.get(options, "information", null)
        console.assert(Helpers.isInteger(index))
        showMediaInformation(index)
    }

    function _playerOptions(options, extraOptions) {
        var playerOpts = Helpers.get(options, "player-options", [])
        return playerOpts.concat(extraOptions)
    }

    function _mlIDList(dataList) {
        var idList = []
        for (var i in dataList) {
            idList.push(dataList[i][idDataRole])
        }

        return idList
    }
}
