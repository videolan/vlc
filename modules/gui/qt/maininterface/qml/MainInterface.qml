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

// NOTE: All available modules used throughout the interface, at least the ones
//       that are unconditionally available, must be imported here as well, so
//       that if they can not be imported the interface can close gracefully:
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
import VLC.Dialogs
// import VLC.MediaLibrary
import VLC.Menus
import VLC.Network
import VLC.PlayerControls

Item {
    id: root

    property bool _extendedFrameVisible: MainCtx.windowSuportExtendedFrame
                                      && MainCtx.clientSideDecoration
                                      && (MainCtx.intfMainWindow.visibility === Window.Windowed)

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
            value: _extendedFrameVisible ? (Qt.platform.pluginName.startsWith("wayland") ? 60 : 30) : 0
        }

        Window.onWindowChanged: {
            if (Window.window && !Qt.colorEqual(Window.window.color, "transparent")) {
                Window.window.color = Qt.binding(function() { return theme.bg.primary })
            }
        }

        DragHandler {
            id: historyDragHandler

            acceptedDevices: PointerDevice.TouchScreen | PointerDevice.TouchPad

            target: null

            minimumPointCount: 2
            maximumPointCount: 2

            grabPermissions: PointerHandler.CanTakeOverFromAnything

            yAxis.enabled: false

            // Qt version check in disguise, only available starting with 6.5:
            enabled: !!xAxis?.activeValueChanged

            property double accumulatedDelta: 0.0

            readonly property real threshold: 80.0

            function onXAxisActiveValueChanged(delta) {
                // We do not want to set enabled based on history condition, because the gesture should be captured
                // regardless to prevent confusion:
                if (History.previousEmpty && (MainCtx.effectiveMainInterfaceMode === MainCtx.MAININTERFACE_MODE_MAINDISPLAY))
                    return

                historyDragHandler.accumulatedDelta = Math.min(threshold, historyDragHandler.accumulatedDelta + delta)
            }

            Component.onCompleted: {
                if (historyDragHandler?.xAxis?.activeValueChanged) {
                    xAxis.activeValueChanged.connect(historyDragHandler, onXAxisActiveValueChanged)
                }
            }

            onActiveChanged: {
                if (!active) {
                    if (historyDragHandler.accumulatedDelta >= threshold) {
                        if (MainCtx.playerView)
                            MainCtx.playerView = false
                        else if (MainCtx.minimalView)
                            MainCtx.minimalView = false
                        else
                            History.previous()
                    }

                    historyDragHandler.accumulatedDelta = 0.0
                    accumulatedDelta = 0.0
                }
            }
        }

        // Going back indicator:
        Rectangle {
            id: goingBackIndicator

            z: 100

            anchors.verticalCenter: parent.verticalCenter

            visible: (historyDragHandler.accumulatedDelta > 0.0)

            scale: Math.min(Math.max(historyDragHandler.accumulatedDelta / historyDragHandler.threshold, 0.2), 1.0)

            readonly property real margin: VLCStyle.dp(128, VLCStyle.scale)

            x: (historyDragHandler.accumulatedDelta / historyDragHandler.threshold * margin)

            implicitWidth: implicitHeight
            implicitHeight: backArrowText.implicitHeight + VLCStyle.margin_small
            radius: (width / 2)

            border.width: 1
            border.color: theme.accent
            border.pixelAligned: (radius < Number.EPSILON)

            color: (historyDragHandler.accumulatedDelta >= historyDragHandler.threshold) ? theme.accent
                                                                                         : theme.bg.primary

            Behavior on color {
                ColorAnimation {
                    duration: VLCStyle.duration_short
                }
            }

            Widgets.IconLabel {
                id: backArrowText

                anchors.centerIn: parent

                text: VLCIcons.back

                font.pixelSize: VLCStyle.fontSize_xxxlarge * 1.2

                color: (historyDragHandler.accumulatedDelta >= historyDragHandler.threshold) ? theme.bg.primary
                                                                                             : theme.accent
            }

            Widgets.RoundedRectangleShadow {
                opacity: (historyDragHandler.accumulatedDelta >= historyDragHandler.threshold) ? 1.0
                                                                                               : 0.0

                color: theme.accent

                blurRadius: VLCStyle.dp(8, VLCStyle.scale)

                compensationFactor: 3.0

                visible: (opacity > 0.0)

                Behavior on opacity {
                    NumberAnimation {
                        duration: VLCStyle.duration_short
                    }
                }
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
            target: Player
            function onPlayingStateChanged() {
                if (Player.playingState === Player.PLAYING_STATE_STOPPED) {
                    MainCtx.playerView = false
                }
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

        Loader {
            id: viewLoader

            anchors.fill: parent

            focus: true
            // If there is depth buffer, clipping is not necessary:
            clip: _extendedFrameVisible && !effect.hasDepthBuffer

            source: {
                // priority applies accross modes
                // minimal > player > medialib
                // MEDIALIB_MODE flag should always be set

                switch (MainCtx.effectiveMainInterfaceMode) {
                case MainCtx.MAININTERFACE_MODE_MINIMAL:
                    return "qrc:///qt/qml/VLC/Player/MinimalView.qml"
                case  MainCtx.MAININTERFACE_MODE_PLAYER:
                    return "qrc:///qt/qml/VLC/Player/Player.qml"
                default:
                    console.error("unexpected interface mode", MainCtx.effectiveMainInterfaceMode)
                case MainCtx.MAININTERFACE_MODE_MAINDISPLAY:
                    return "qrc:///qt/qml/VLC/MainInterface/MainDisplay.qml"
                }
            }
        }

        Loader {
            asynchronous: true
            source: "qrc:///qt/qml/VLC/Menus/GlobalShortcuts.qml"
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
        hollow: (z >= 0) || (Window.window && (Window.window.color.a < 1.0)) // the interface may be translucent if the window has backdrop blur
        // No need for blending, even if this is above everything (when there is depth buffer). This item does not need to be blended in the scene
        // graph. The system compositor is still going to respect the transparency when compositing the window. By disabling blending, this is treated
        // as opaque in the scene graph by the renderer, which makes it possible to not use a clip node for the content due to depth test (since
        // this item is quasi-opaque, the content pixels obscured by the shadow pixels are not painted), and at the same time not spend effort on
        // blending. Note that this optimization relies on hollow mode that discards the inner pixels, so the actual content area is still painted.
        blending: false
        visible: _extendedFrameVisible && !MainCtx.platformHandlesShadowsWithCSD()
        color: Qt.rgba(0.0, 0.0, 0.0, 0.5) // sg opacity < 1.0 force enables blending, so we adjust the color instead

        // If there is depth buffer, we enable hollow mode. The inner area is discarded, so we can do this:
        z: hasDepthBuffer ? 99 : -1

        readonly property bool hasDepthBuffer: (Window.window && MainCtx.windowHasDepthBuffer(Window.window))

        // Blur radius can not be greater than (margin / compensationFactor), as in that case it would need bigger
        // size than the window to compensate. If you want bigger blur radius, either decrease the compensation
        // factor which can lead to visible clipping, or increase the window extended margin:
        blurRadius: (MainCtx.windowExtendedMargin / effect.compensationFactor)

        // 2.0 (default) compensation factor makes clipping obvious, especially with white background
        implicitCompensationFactor: 3.0

        compensationFactor: MainCtx.intfMainWindow.active ? (implicitCompensationFactor)
                                                          : (implicitCompensationFactor * 2)

        Behavior on compensationFactor {
            // FIXME: Use UniformAnimator instead
            NumberAnimation {
                duration: VLCStyle.duration_veryShort
            }
        }
    }
}
