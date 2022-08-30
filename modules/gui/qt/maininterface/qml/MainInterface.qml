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
import QtQuick.Layouts 1.11
import QtQuick.Controls 2.4
import QtQuick.Window 2.11

import org.videolan.vlc 0.1
import org.videolan.compat 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

import "qrc:///dialogs/" as DG
import "qrc:///playlist/" as PL

Item {
    id: root

    property bool _interfaceReady: false
    property bool _playlistReady: false

    property alias g_root: root
    property QtObject g_dialogs: dialogsLoader.item

    BindingCompat {
        target: VLCStyle.self
        property: "appWidth"
        value: root.width
    }

    BindingCompat {
        target: VLCStyle.self
        property: "appHeight"
        value: root.height
    }

    BindingCompat {
        target: root.Window.window
        property: "color"
        value: VLCStyle.colors.bg
        when: root.Window.window &&
              !Qt.colorEqual(root.Window.window.color,
                             "transparent")
    }

    Widgets.ToolTipExt {
        id: attachedToolTip

        parent: null
        z: 99
        colors: parent && parent.colors ? parent.colors
                                        : VLCStyle.colors

        Component.onCompleted: {
            MainCtx.setAttachedToolTip(this)
        }
    }

    Loader {
        id: playlistWindowLoader
        asynchronous: true
        active: !MainCtx.playlistDocked && MainCtx.playlistVisible
        source: "qrc:///playlist/PlaylistDetachedWindow.qml"
    }
    Connections {
        target: playlistWindowLoader.item
        onClosing: MainCtx.playlistVisible = false
    }


    PlaylistControllerModel {
        id: mainPlaylistController
        playlistPtr: MainCtx.mainPlaylist

        onPlaylistInitialized: {
            root._playlistReady = true
            if (root._interfaceReady)
                setInitialView()
        }
    }

    readonly property var pageModel: [
        { name: "about", url: "qrc:///about/About.qml" },
        { name: "mc", url: "qrc:///main/MainDisplay.qml" },
        { name: "player", url:"qrc:///player/Player.qml" },
    ]

    function loadCurrentHistoryView() {
        var current = History.current
        if ( !current || !current.name  || !current.properties ) {
            console.warn("unable to load requested view, undefined")
            return
        }
        stackView.loadView(root.pageModel, current.name, current.properties)
    }

    Connections {
        target: History
        onCurrentChanged: loadCurrentHistoryView()
    }

    function setInitialView() {
        //set the initial view
        var loadPlayer = !mainPlaylistController.empty;

        if (MainCtx.mediaLibraryAvailable)
            History.push(["mc", "video"], loadPlayer ? History.Stay : History.Go)
        else
            History.push(["mc", "home"], loadPlayer ? History.Stay : History.Go)

        if (loadPlayer)
            History.push(["player"])
    }


    Component.onCompleted: {
        root._interfaceReady = true;
        if (root._playlistReady)
            setInitialView()
    }


    DropArea {
        anchors.fill: parent
        onDropped: {
            var urls = []
            if (drop.hasUrls) {

                for (var i = 0; i < drop.urls.length; i++)
                    urls.push(drop.urls[i])

            } else if (drop.hasText) {
                /* Browsers give content as text if you dnd the addressbar,
                   so check if mimedata has valid url in text and use it
                   if we didn't get any normal Urls()*/

                urls.push(drop.text)
            }

            if (urls.length > 0) {
                /* D&D of a subtitles file, add it on the fly */
                if (Player.isPlaying && urls.length == 1) {
                    if (Player.associateSubtitleFile(urls[0])) {
                        drop.accept()
                        return
                    }
                }

                mainPlaylistController.append(urls, true)
                drop.accept()
            }
        }

        Widgets.StackViewExt {
            id: stackView
            anchors.fill: parent
            focus: true

            Connections {
                target: Player
                onPlayingStateChanged: {
                    if (Player.playingState === Player.PLAYING_STATE_STOPPED
                            && History.current.name === "player") {
                        if (History.previousEmpty)
                        {
                            if (MainCtx.mediaLibraryAvailable)
                                History.push(["mc", "video"])
                            else
                                History.push(["mc", "home"])
                        }
                        else
                            History.previous()
                    }
                }
            }
        }
    }

    Loader {
        asynchronous: true
        source: "qrc:///menus/GlobalShortcuts.qml"
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

    MouseArea {
        /// handles mouse navigation buttons
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        cursorShape: undefined
        onClicked: History.previous()
    }

    Loader {
        active: {
            var windowVisibility = MainCtx.intfMainWindow.visibility
            return MainCtx.clientSideDecoration
                    && (windowVisibility !== Window.Maximized)
                    && (windowVisibility !== Window.FullScreen)

        }

        source: "qrc:///widgets/CSDMouseStealer.qml"
    }

}
