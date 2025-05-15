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

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util
import VLC.Playlist
import VLC.Player

Item {
    id: root

    property bool _interfaceReady: false
    property bool _playlistReady: false
    property bool _extendedFrameVisible: MainCtx.windowSuportExtendedFrame
                                      && MainCtx.clientSideDecoration
                                      && (MainCtx.intfMainWindow.visibility === Window.Windowed)

    //when exiting minimal mode, what is the page to restore
    property bool _minimalRestorePlayer: false

    readonly property var _pageModel: [
        { name: "mc", url: "qrc:///qt/qml/VLC/MainInterface/MainDisplay.qml" },
        { name: "player", url:"qrc:///qt/qml/VLC/Player/Player.qml" },
        { name: "minimal", url:"qrc:///qt/qml/VLC/Player/MinimalView.qml" },
    ]

    property var _oldHistoryPath: ([])

    function setInitialView() {
        //set the initial view
        if (!MainPlaylistController.empty)
            MainCtx.requestShowPlayerView()
        else
            MainCtx.requestShowMainView()
    }

    function loadCurrentHistoryView(focusReason) {
        contextSaver.save(_oldHistoryPath)

        stackView.loadView(History.viewPath, History.viewProp, focusReason)

        contextSaver.restore(History.viewPath)
        _oldHistoryPath = History.viewPath
    }

    ModelSortSettingHandler {
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
            value: _extendedFrameVisible ? (Qt.platform.pluginName.startsWith("wayland") ? 40 : 20) : 0
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
            source: "qrc:///qt/qml/VLC/Playlist/PlaylistDetachedWindow.qml"
        }

        Connections {
            target: MainPlaylistController

            function onInitializedChanged() {
                console.assert(MainPlaylistController.initialized)
                if (root._interfaceReady && !root._playlistReady) {
                    root._playlistReady = true
                    setInitialView()
                }
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

            function onRequestShowMainView() {
                root._minimalRestorePlayer = false
                if (MainCtx.minimalView)
                    return

                if (History.match(History.viewPath, ["mc"]))
                    return

                if (MainCtx.hasEmbededVideo && MainCtx.canShowVideoPIP === false)
                    MainPlaylistController.stop()

                if (History.previousEmpty) {
                    History.update(["mc", "home"])
                    loadCurrentHistoryView(Qt.OtherFocusReason)
                } else
                    History.previous()
            }

            function onRequestShowPlayerView() {
                root._minimalRestorePlayer = true
                if (MainCtx.minimalView)
                    return

                if (!History.match(History.viewPath, ["player"]))
                    History.push(["player"])
            }

            function onMinimalViewChanged() {
                const isCurrentlyMinimal = History.match(History.viewPath, ["minimal"])

                if (MainCtx.minimalView && !isCurrentlyMinimal) {
                    const isCurrentlyPlayer = History.match(History.viewPath, ["player"])
                    if (isCurrentlyPlayer) {
                        History.update(["minimal"])
                        loadCurrentHistoryView(Qt.OtherFocusReason)
                    } else {
                        History.push(["minimal"])
                    }
                } else if (!MainCtx.minimalView && isCurrentlyMinimal) {
                    if (root._minimalRestorePlayer) {
                        History.update(["player"])
                        loadCurrentHistoryView(Qt.OtherFocusReason)
                    } else {
                        History.previous()
                    }
                }
            }
        }

        Component.onCompleted: {
            root._interfaceReady = true
            if (!root._playlistReady && MainPlaylistController.initialized) {
                root._playlistReady = true
                setInitialView()
            }
        }

        DropArea {
            anchors.fill: parent
            z: -1

            onEntered: (drag) => {
                // Do not handle internal drag here:
                if (!drag.source) {
                    // Foreign drag, check if valid:
                    if (drag.hasUrls || drag.hasText) {
                        drag.accepted = true
                        return
                    }
                }

                drag.accepted = false
            }

            onDropped: (drop) => {
                let urls = []
                if (drop.hasUrls) {

                    for (let i = 0; i < drop.urls.length; i++)
                        urls.push(drop.urls[i])

                } else if (drop.hasText) {
                    /* Browsers give content as text if you dnd the addressbar,
                       so check if mimedata has valid url in text and use it
                       if we didn't get any normal Urls()*/

                    if (drop.text.includes("\n")) {
                        const normalizedLineEndingsDropText = drop.text.replace("\r\n", "\n")
                        urls.push(...normalizedLineEndingsDropText.split("\n"))
                    } else {
                        urls.push(drop.text)
                    }
                }

                if (urls.length > 0) {
                    /* D&D of a subtitles file, add it on the fly */
                    if (Player.isStarted && urls.length == 1) {
                        if (Player.associateSubtitleFile(urls[0])) {
                            drop.accept()
                            return
                        }
                    }

                    MainPlaylistController.append(urls, true)
                    drop.accept()
                }
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
                    if (Player.playingState === Player.PLAYING_STATE_STOPPED) {
                        MainCtx.requestShowMainView()
                    }
                }
            }
        }

        Loader {
            asynchronous: true
            source: "qrc:///qt/qml/VLC/Menus/GlobalShortcuts.qml"
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
                return MainCtx.clientSideDecoration && !MainCtx.platformHandlesResizeWithCSD()
                        && (windowVisibility !== Window.Maximized)
                        && (windowVisibility !== Window.FullScreen)

            }
            Component.onCompleted: {
                setSource(
                    "qrc:///qt/qml/VLC/Widgets/CSDMouseStealer.qml", {
                        target: g_mainInterface,
                        anchorInside: Qt.binding(() => !_extendedFrameVisible)
                    })
            }
        }
    }

    //draw the window drop shadow ourselve when the windowing system doesn't
    //provide them but support extended frame
    Widgets.RoundedRectangleShadow {
        id: effect
        parent: g_mainInterface
        hollow: Window.window && (Window.window.color.a < 1.0) // the interface may be translucent if the window has backdrop blur
        blending: false // stacked below everything, no need for blending even though it is not opaque
        visible: _extendedFrameVisible && !MainCtx.platformHandlesShadowsWithCSD()
        opacity:  0.5

        // Blur radius can not be greater than (margin / compensationFactor), as in that case it would need bigger
        // size than the window to compensate. If you want bigger blur radius, either decrease the compensation
        // factor which can lead to visible clipping, or increase the window extended margin:
        property real activeBlurRadius: (MainCtx.windowExtendedMargin / effect.compensationFactor)

        blurRadius: MainCtx.intfMainWindow.active ? activeBlurRadius
                                                  : (activeBlurRadius / 2.0)

        Behavior on blurRadius {
            // FIXME: Use UniformAnimator instead
            NumberAnimation {
                duration: VLCStyle.duration_veryShort
            }
        }
    }
}
