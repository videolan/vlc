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
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

import "qrc:///dialogs/" as DG
import "qrc:///playlist/" as PL

Rectangle {
    id: root
    color: "transparent"
    property bool _interfaceReady: false
    property bool _playlistReady: false

    property alias mainInterfaceRect: root
    property variant g_dialogs: dialogsLoader.item

    Binding {
        target: VLCStyle.self
        property: "appWidth"
        value: root.width
    }

    Binding {
        target: VLCStyle.self
        property: "appHeight"
        value: root.height
    }

    Loader {
        id: playlistWindowLoader
        asynchronous: true
        active: !mainInterface.playlistDocked && mainInterface.playlistVisible
        source: "qrc:///playlist/PlaylistDetachedWindow.qml"
    }
    Connections {
        target: playlistWindowLoader.item
        onClosing: mainInterface.playlistVisible = false
    }


    PlaylistControllerModel {
        id: mainPlaylistController
        playlistPtr: mainctx.playlist

        onPlaylistInitialized: {
            root._playlistReady = true
            if (root._interfaceReady)
                setInitialView()
        }
    }

    readonly property var pageModel: [
        { name: "about", url: "qrc:///about/About.qml" },
        { name: "mc", url: "qrc:///medialibrary/MainDisplay.qml" },
        { name: "playlist", url: "qrc:///playlist/PlaylistMainView.qml" },
        { name: "player", url:"qrc:///player/Player.qml" },
    ]

    function loadCurrentHistoryView() {
        var current = history.current
        if ( !current || !current.view ) {
            console.warn("unable to load requested view, undefined")
            return
        }
        stackView.loadView(root.pageModel, current.view, current.viewProperties)
    }

    Connections {
        target: history
        onCurrentChanged: loadCurrentHistoryView()
    }

    function setInitialView() {
        //set the initial view
        if (!mainPlaylistController.empty)
            history.push(["player"])
        else
        {
            if (medialib)
                history.push(["mc", "video"])
            else
                history.push(["playlist"])
        }
    }


    Component.onCompleted: {
        root._interfaceReady = true;
        if (root._playlistReady)
            setInitialView()
    }


    DropArea {
        anchors.fill: parent
        onEntered: console.log("drop Enter")
        onExited: console.log("drop exit")
        onDropped: {
            if (drop.hasUrls) {
                var list = []
                for (var i = 0; i < drop.urls.length; i++){
                    list.push(drop.urls[i])
                }
                mainPlaylistController.append(list, true)
                drop.accept()
            }
        }

        Widgets.StackViewExt {
            id: stackView
            anchors.fill: parent
            focus: true

            Connections {
                target: player
                onPlayingStateChanged: {
                    if (player.playingState === PlayerController.PLAYING_STATE_STOPPED
                            && history.current.view === "player") {
                        if (history.previousEmpty)
                        {
                            if (medialib)
                                history.push(["mc", "video"])
                            else
                                history.push(["playlist"])
                        }
                        else
                            history.previous()
                    }
                }
            }

            Connections {
                target: player.videoTracks
                onDataChanged: {
                    if (player.videoTracks.rowCount() > 0
                            && player.playingState === PlayerController.PLAYING_STATE_PLAYING
                            && history.current.view !== "player") {
                        history.push(["player"])
                    }
                }
            }
        }
    }

    Loader {
        id: dialogsLoader

        anchors.fill: parent
        asynchronous: true
        source: "qrc:///dialogs/Dialogs.qml"

        onLoaded:  {
            item.bgContent = root
        }
    }
    Connections {
        target: dialogsLoader.item
        onRestoreFocus: {
            stackView.focus = true
        }
    }
}
