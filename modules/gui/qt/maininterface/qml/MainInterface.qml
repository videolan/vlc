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

// NOTE: All imports used throughout the interface
//       must be imported here as well:
import QtQml
import QtQml.Models
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates
import QtQuick.Controls
import QtQuick.Window
import Qt5Compat.GraphicalEffects

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/" as Util
import "qrc:///playlist/" as PL

Item {
    id: root

    property bool _interfaceReady: false
    property bool _playlistReady: false
    property bool _extendedFrameVisible: MainCtx.windowSuportExtendedFrame
                                      && MainCtx.clientSideDecoration
                                      && (MainCtx.intfMainWindow.visibility === Window.Windowed)

    readonly property var _pageModel: [
        { name: "mc", url: "qrc:///main/MainDisplay.qml" },
        { name: "player", url:"qrc:///player/Player.qml" },
    ]

    property var _oldHistoryPath: ([])

    function setInitialView() {
        //set the initial view
        const loadPlayer = !MainPlaylistController.empty;

        if (loadPlayer)
        {
            if (MainCtx.mediaLibraryAvailable)
                History.update(["mc", "video"])
            else
                History.update(["mc", "home"])
            History.push(["player"])
        }
        else
            _pushHome()
    }

    function _pushHome() {
        if (MainCtx.mediaLibraryAvailable)
            History.push(["mc", "video"])
        else
            History.push(["mc", "home"])
    }

    function loadCurrentHistoryView(focusReason) {
        contextSaver.save(_oldHistoryPath)

        stackView.loadView(History.viewPath, History.viewProp, focusReason)

        contextSaver.restore(History.viewPath)
        _oldHistoryPath = History.viewPath
    }

    Util.ModelSortSettingHandler {
        id: contextSaver
    }

    Item {
        id: g_mainInterface

        anchors.fill: parent
        anchors.topMargin: MainCtx.windowExtendedMargin
        anchors.leftMargin: MainCtx.windowExtendedMargin
        anchors.rightMargin: MainCtx.windowExtendedMargin
        anchors.bottomMargin: MainCtx.windowExtendedMargin

        Binding {
            target: VLCStyle
            property: "appWidth"
            value: g_mainInterface.width
        }

        Binding {
            target: VLCStyle
            property: "appHeight"
            value: g_mainInterface.height
        }

        Binding {
            target: MainCtx
            property: "windowExtendedMargin"
            value: _extendedFrameVisible ? VLCStyle.dp(20, VLCStyle.scale) : 0
        }

        Window.onWindowChanged: {
            if (Window.window && !Qt.colorEqual(Window.window.color, "transparent")) {
                Window.window.color = Qt.binding(function() { return theme.bg.primary })
            }
        }

        ColorContext {
            id: theme
            palette: VLCStyle.palette
            colorSet: ColorContext.View
        }

        Widgets.ToolTipExt {
            id: attachedToolTip

            parent: null
            z: 99
            colorContext.palette: parent && parent.colorContext ? parent.colorContext.palette
                                                                : VLCStyle.palette

            Component.onCompleted: {
                MainCtx.setAttachedToolTip(this)
            }
        }

        Loader {
            id: playlistWindowLoader
            asynchronous: true
            active: !MainCtx.playlistDocked
            source: "qrc:///playlist/PlaylistDetachedWindow.qml"
        }

        Connections {
            target: MainPlaylistController

            function onPlaylistInitialized() {
                _playlistReady = true
                if (_interfaceReady)
                    setInitialView()
            }
        }

        Connections {
            target: History
            function onNavigate(focusReason) {
                loadCurrentHistoryView(focusReason)
                MainCtx.mediaLibraryVisible = !History.match(History.viewPath, ["player"])
            }
        }

        Connections {
            target: MainCtx

            function onMediaLibraryVisibleChanged() {
                if (MainCtx.mediaLibraryVisible) {
                    if (History.match(History.viewPath, ["mc"]))
                        return

                    // NOTE: Useful when we started the application on the 'player' view.
                    if (History.previousEmpty) {
                        if (MainCtx.hasEmbededVideo && MainCtx.canShowVideoPIP === false)
                            MainPlaylistController.stop()

                        _pushHome()

                        return
                    }

                    if (MainCtx.hasEmbededVideo && MainCtx.canShowVideoPIP === false)
                        MainPlaylistController.stop()

                    History.previous()
                } else {
                    if (History.match(History.viewPath, ["player"]))
                        return

                    History.push(["player"])
                }
            }
        }

        Component.onCompleted: {
            _interfaceReady = true;
            if (_playlistReady)
                setInitialView()
        }


        DropArea {
            anchors.fill: parent
            onDropped: (drop) => {
                let urls = []
                if (drop.hasUrls) {

                    for (let i = 0; i < drop.urls.length; i++)
                    {
                        /* First decode the URL since we'll re-encode it
                           afterwards, while fixing the non-encoded spaces. */
                        let url = decodeURIComponent(drop.urls[i]);
                        urls.push(url);
                    }

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

                    MainPlaylistController.append(urls, true)
                    drop.accept()
                }
            }

            Widgets.PageLoader {
                id: stackView
                anchors.fill: parent
                focus: true
                clip: _extendedFrameVisible

                pageModel: _pageModel

                Connections {
                    target: Player
                    function onPlayingStateChanged() {
                        if (Player.playingState === Player.PLAYING_STATE_STOPPED
                                && History.match(History.viewPath, ["player"]) ) {
                            if (History.previousEmpty)
                                _pushHome()
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

        MouseArea {
            /// handles mouse navigation buttons
            anchors.fill: parent
            acceptedButtons: Qt.BackButton
            cursorShape: undefined
            onClicked: History.previous()
        }

        Loader {
            active: {
                const windowVisibility = MainCtx.intfMainWindow.visibility
                return MainCtx.clientSideDecoration
                        && (windowVisibility !== Window.Maximized)
                        && (windowVisibility !== Window.FullScreen)

            }

            source: "qrc:///widgets/CSDMouseStealer.qml"

            onLoaded: {
                item.target = g_mainInterface
                item.anchorInside = Qt.binding(() => !_extendedFrameVisible)
            }
        }
    }

    //draw the window drop shadow ourselve when the windowing system doesn't
    //provide them but support extended frame
    RectangularGlow {
        id: effect
        z: -1
        visible: _extendedFrameVisible
        anchors.fill: g_mainInterface
        spread: 0.0
        color: "black"
        opacity:  0.5
        cornerRadius: glowRadius
        states: [
            State {
                when: MainCtx.intfMainWindow.active
                PropertyChanges {
                    target: effect
                    glowRadius: MainCtx.windowExtendedMargin * 0.7
                }
            },
            State {
                when: !MainCtx.intfMainWindow.active
                PropertyChanges {
                    target: effect
                    glowRadius: MainCtx.windowExtendedMargin * 0.5
                }
            }
        ]
        Behavior on  glowRadius {
            NumberAnimation {
                duration: VLCStyle.duration_veryShort
            }
        }
    }
}
